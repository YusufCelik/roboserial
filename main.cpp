//
//  main.cpp
//  roboserial
//
//  Created by Yusuf Celik on 13/06/2020.
//  Copyright © 2020 Robot Hermes. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <ftdi.h>
#include <unistd.h>
#include <string.h>

static long calculate_file_size(FILE *file) {
    long exe_size =  0;
    
    fseek(file, 0, SEEK_END);
    exe_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    return exe_size;
}

static int calculate_checksum(FILE *file) {
    long exe_size =  calculate_file_size(file);
    int checksum = 0;
    
    fseek(file, 0x800, SEEK_SET);
    
    for (int x = 0x800; x < exe_size; x++)
    {
        unsigned char b;
        fread(&b, 1, 1, file);
        
        checksum += (unsigned int)b;
    }
    
    return checksum;
}

static void write_header(FILE *file, ftdi_context *&ftdi) {
    unsigned char header[2048];
    
    fseek(file, 0, SEEK_SET);
    fread(&header, sizeof(unsigned char), 2048, file);
    ftdi_write_data(ftdi, header, 2048);
    
    printf("Wrote header \n");
}

static void write_jmp_addr(FILE *file, ftdi_context *&ftdi) {
    unsigned char jmp_addr[4];
    
    fseek(file, 16, SEEK_SET);
    fread(&jmp_addr, sizeof(unsigned char), 4, file);
    ftdi_write_data(ftdi, jmp_addr, 4);
    
    printf("Wrote jump address \n");
}

static void write_wr_addr(FILE *file, ftdi_context *&ftdi) {
    unsigned char write_addr[4];
    
    fseek(file, 24, SEEK_SET);
    fread(&write_addr, sizeof(unsigned char), 4, file);
    ftdi_write_data(ftdi, write_addr, 4);
    
    printf("Wrote write address \n");
}

static void write_file_size(FILE *file, ftdi_context *&ftdi) {
    unsigned char file_size[4];
    
    fseek(file, 28, SEEK_SET);
    fread(&file_size, sizeof(unsigned char), 4, file);
    ftdi_write_data(ftdi, file_size, 4);
    
    printf("Wrote file size \n");
}

static void write_checksum(FILE *file, ftdi_context *&ftdi) {
    int checksum = calculate_checksum(file);
 
    ftdi_write_data(ftdi,  (const unsigned char *)&checksum, 4);
    
    printf("Wrote checksum \n");
}

static void write_ps_executable(FILE *file, ftdi_context *&ftdi) {
    long exe_size =  calculate_file_size(file);
    long chunks = (exe_size - 2048) / 2048;
    
    fseek(file, 2048, SEEK_SET);
    
    for (int x = 0; x < chunks; x++)
    {
      unsigned char b[2048];
      fread(&b, 1, 2048, file);
    
      ftdi_write_data(ftdi, b, 2048);

      printf("written packet %d \n", x);
    }
}

static void push_exe_command(ftdi_context *&ftdi) {
    unsigned char start_cmd[4] = {'S', 'E', 'X', 'E'};
    
    ftdi_write_data(ftdi, start_cmd, 4);
    usleep(300000);
}

static void push_fast_command(ftdi_context *&ftdi) {
    unsigned char fast_cmd[4] = {'F', 'A', 'S', 'T'};
    ftdi_write_data(ftdi, fast_cmd, 4);
    usleep(300000);
    
    ftdi_set_baudrate(ftdi, 510000);
}

static int init_ftdi(ftdi_context *&ftdi) {
    int ret;
    
    struct ftdi_version_info version;
     if ((ftdi = ftdi_new()) == 0)
    {
         fprintf(stderr, "ftdi_new failed\n");
         return EXIT_FAILURE;
     }

     version = ftdi_get_library_version();
     printf("Initialized libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n",
         version.version_str, version.major, version.minor, version.micro,
         version.snapshot_str);

     if ((ret = ftdi_usb_open(ftdi, 0x0403, 0x6001)) < 0)
     {
         fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
         ftdi_free(ftdi);
         return EXIT_FAILURE;
     }

     ftdi_set_baudrate(ftdi, 115200);
     ftdi_set_line_property(ftdi, BITS_8, STOP_BIT_1, NONE);
     ftdi_setflowctrl(ftdi, SIO_DISABLE_FLOW_CTRL);
     ftdi_write_data_set_chunksize(ftdi, 2048);
     ftdi_read_data_set_chunksize(ftdi, 1);
     
     // Read out FTDIChip-ID of R type chips
     if (ftdi->type == TYPE_R)
     {
         unsigned int chipid;
         printf("ftdi_read_chipid: %d\n", ftdi_read_chipid(ftdi, &chipid));
         printf("FTDI chipid: %X\n", chipid);
     }
    
     return EXIT_SUCCESS;
}

static int close_ftdi(ftdi_context *&ftdi) {
    int ret;

    ftdi_usb_purge_buffers(ftdi);

    if ((ret = ftdi_usb_close(ftdi)) < 0)
    {
        fprintf(stderr, "unable to close ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }
    
     return EXIT_SUCCESS;
}

static void render_header() {
    const char* header = "\
    +---------------------------------------------------------------------------------+\n\
    |                                                                                 |\n\
    |                                                                                 |\n\
    |   Robo serial 0.1                                                               |\n\
    |                                                                                 |\n\
    |   Usage: args filename                                                          |\n\
    |                                                                                 |\n\
    |   e.g. /f /exe ./psx_helloworld.exe                                             |\n\
    |                                                                                 |\n\
    |   args:                                                                         |\n\
    |                                                                                 |\n\
    |     /f fast mode (510000 bps)                                                   |\n\
    |     /exe load exe                                                               |\n\
    |                                                                                 |\n\
    |                                                                                 |\n\
    |                                                                                 |\n\
    |                                                                                 |\n\
    +---------------------------------------------------------------------------------+\n";
    
    printf("%s", header);
}

int main(int argc, char *argv[])
{
    struct ftdi_context *ftdi;
    init_ftdi(ftdi);

    render_header();

    FILE *file = fopen(argv[argc - 1], "rb");
    
    if (file == NULL) {
        printf("File not found \n");
        return EXIT_FAILURE;
    }
    
    for (int x = 0; x < argc; x++) {
        if (strncmp(argv[x], "/f", 2) == 0) {
            push_fast_command(ftdi);
        }
        
        if (strncmp(argv[x], "/exe", 4) == 0) {
            push_exe_command(ftdi);
        }
    }
    
    write_header(file, ftdi);
    write_jmp_addr(file, ftdi);
    write_wr_addr(file, ftdi);
    write_file_size(file, ftdi);
    write_checksum(file, ftdi);
    write_ps_executable(file, ftdi);
    
    fclose(file);
    close_ftdi(ftdi);
}
