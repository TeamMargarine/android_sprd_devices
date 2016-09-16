/*
 * phy address simple read, word only, return to stdout
 *
 * 2010.11.16 v01 fool2
 *          init version
 * 2010.11.17 v02 fool2
 *          add ADI reg read support
 * 2010.11.17 v03 fool2
 *          add continous area read support
 * 2010.11.17 v04 fool2
 *          add set support, polish the help info
 * 2012.11.16 v05 fool2
 *          add sc8825 (tiger) a-die regs access
 * 2013.4.1 v06 fool2
 *          add sc7710g /sc8830(shark) a-die regs access
 */

#define LOG_NDEBUG 0
#define LOG_TAG "lookat"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <getopt.h>

#define MAPSIZE (4096)
unsigned int ana_max_size = 4096;
unsigned int ana_reg_addr_start = 0x42000000, ana_reg_addr_end = 0;
#define IS_ANA_ADDR(_a) (_a>=ana_reg_addr_start && _a<=ana_reg_addr_end)

#define ADI_ARM_RD_CMD(b) (b+0x24)
#define ADI_RD_DATA(b)    (b+0x28)
#define ADI_FIFO_STS(b)   (b+0x2C)

/* option flags */
#define LOOKAT_F_GET      (1<<0)
#define LOOKAT_F_GET_MUL  (1<<1)
#define LOOKAT_F_SET      (1<<2)

/* #define DEBUG */
#ifdef DEBUG
#define DEBUGSEE fprintf
#else
#define DEBUGSEE(...)
#endif

void usage(void)
{
        fprintf(stderr,
"\n"
"Usage: lookat [-l nword] [-s value] [-h] phy_addr_in_hex\n"
"get/set the value of IO reg at phy_addr_in_hex\n"
"  -l nword     print values of nword continous regs start from \n"
"               phy_addr_in_hex in a formated way\n"
"  -s value     set a reg at phy_addr_in_hex to value\n"
"  -h           print this message\n\n"
" NOTE:\n"
" * the phy_addr_in_hex should be 4-byte aligned in hex form\n"
" * the value should be in hex form too\n"
" * currently, there are two reg areas:\n"
"  1) 0x%08x-0x%08x - ADI REG area\n"
"  2) other addresses       - Other IO areas\n"
"\n"
                ,ana_reg_addr_start, ana_reg_addr_end);
}

unsigned short ADI_Analogdie_reg_read (unsigned int vbase, unsigned int paddr)
{
        unsigned int data;

        *(volatile unsigned int*)(ADI_ARM_RD_CMD(vbase)) = paddr;

        do {

                 data = *(volatile unsigned int*)(ADI_RD_DATA(vbase));
        } while (data & 0x80000000);

        if ((data & 0xffff0000) != (paddr << 16)) {
                fprintf(stderr, "ADI read error [0x%x]\n", data);
                exit(EXIT_FAILURE);
        }

        return ((unsigned short)(data & 0xffff));
}

void ADI_Analogdie_reg_write (unsigned int vbase, unsigned int vaddr,
                                        unsigned int data)
{

        do {

                 if ((*(volatile unsigned int *)(ADI_FIFO_STS(vbase))&(1<<10))
                          != 0)
                         break;
        } while (1);

        *(volatile unsigned int*)(vaddr) = (data & 0xffff);

}

void set_value(unsigned int paddr, unsigned int vbase,
                unsigned int offset, unsigned int value)
{
        unsigned int old_value, new_value;

        if (IS_ANA_ADDR(paddr)) {
                old_value = ADI_Analogdie_reg_read(vbase,paddr);
                ADI_Analogdie_reg_write(vbase,(vbase+offset),value);
                new_value = ADI_Analogdie_reg_read(vbase,paddr);
                DEBUGSEE(stdout, "ADie REG\n"
                                "before: 0x%04x\n"
                                "after:  0x%04x\n",
                                old_value, new_value);

        }
        else {
                old_value = *((unsigned int *)(vbase+offset));
                *(volatile unsigned int*)(vbase+offset) = value;
                new_value = *((unsigned int *)(vbase+offset));
                DEBUGSEE(stdout, "before: 0x%08x\n"
                                "after:  0x%08x\n",
                                old_value, new_value);
        }

        return;
}

void print_result(unsigned int paddr, unsigned int vbase,
                unsigned int offset)
{
        if (IS_ANA_ADDR(paddr))
                fprintf(stdout, "0x%04x\n",
                                ADI_Analogdie_reg_read(vbase,paddr));
        else 
                fprintf(stdout, "0x%08x\n", 
                                *((unsigned int *)(vbase+offset)));
        return;
}

void print_result_mul(unsigned int paddr, unsigned int vbase,
                unsigned int offset, int nword)
{
        int i;

        /* print a serial of reg in a formated way */
        fprintf(stdout, "  ADDRESS  |   VALUE\n"
                        "-----------+-----------\n");
        for (i=0;i<nword;i++) {
                if (IS_ANA_ADDR(paddr))
                        fprintf(stdout, "0x%08x |     0x%04x\n", paddr,
                                        ADI_Analogdie_reg_read(vbase,paddr));
                else 
                        fprintf(stdout, "0x%08x | 0x%08x\n", paddr,
                                        *((unsigned int *)(vbase+offset)));
                paddr+=4;
                offset+=4;
        }

        return;
}

int chip_probe(void)
{
    int fd;
    char *vbase;
    unsigned int chip_id;
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        perror("could not open /dev/mem/");
        exit(EXIT_FAILURE);
    }

    vbase = (char*)mmap(0, MAPSIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0x20900000);
    if (vbase == (char *)-1) {
        perror("mmap failed!");
        exit(EXIT_FAILURE);
    }

    chip_id = *((unsigned int *)(vbase+0x3fc));
    chip_id >>= 16;

    if (chip_id == 0x8810 || chip_id == 0x7710) {
        ana_reg_addr_start = 0x82000040;
    }
    else if (chip_id == 0x8820 || chip_id == 0x8830) {/* tiger, shark...*/
        ana_reg_addr_start = 0x42000040;
    }
    else {
        perror("unknown chip!");
//        exit(EXIT_FAILURE);
    }

    /* each adi frame only contain 9~10 bits address */
    ana_reg_addr_end = (ana_reg_addr_start & ~(MAPSIZE - 1)) + ana_max_size;

    if (munmap(vbase, 4096) == -1) {
        perror("munmap failed!");
        exit(EXIT_FAILURE);
    }

    close(fd);
    return 0;
}

int main (int argc, char** argv)
{
        int fd, opt;
        int flags = 0; /* option indicator */
        unsigned int paddr, pbase, offset;
        unsigned int value = 0, nword = 0;
        char *vbase;

        chip_probe();

        while ((opt = getopt(argc, argv, "s:l:h")) != -1) {
                switch (opt) {
                case 's':
                        flags |= LOOKAT_F_SET;
                        value = strtoul(optarg, NULL, 16);
                        DEBUGSEE(stdout, "[I] value = 0x%x\n", value);
                        break;
                case 'l':
                        flags |= LOOKAT_F_GET_MUL;
                        nword = strtoul(optarg, NULL, 10);
                        DEBUGSEE(stdout, "[I] nword = 0x%d\n", nword);
                        if (nword <= 0) nword = 1;
                        break;
                case 'h':
                        usage();
                        exit(0);
                default:
                        usage();
                        exit(EXIT_FAILURE);
                }
        }

        if (optind == 1) 
                flags |= LOOKAT_F_GET;

        if (optind >= argc) {
                fprintf(stderr, "Expected argument after options\n");
                exit(EXIT_FAILURE);
        }

        paddr = strtoul(argv[optind], NULL, 16);
        DEBUGSEE(stdout, "[I] paddr = 0x%x\n", paddr);

        if (paddr&0x3) {
                fprintf(stderr, "physical address should be 4-byte aligned\n");
                exit(EXIT_FAILURE);
        }

        offset = paddr & (MAPSIZE - 1);
        pbase = paddr - offset;

        if ((nword<<2) >= (MAPSIZE-offset)) {
                fprintf(stderr, "length should not cross the page boundary!\n");
                exit(EXIT_FAILURE);
        }

        /* ok, we've done with the options/arguments, start to work */
        if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
                perror("could not open /dev/mem/");
                exit(EXIT_FAILURE);
        }

        vbase = (char*)mmap(0, MAPSIZE, PROT_READ | PROT_WRITE, 
                        MAP_SHARED, fd, pbase);
        if (vbase == (char *)-1) {
                perror("mmap failed!");
                exit(EXIT_FAILURE);
        }

        if (flags & LOOKAT_F_SET )
                set_value(paddr, (unsigned int)vbase, offset, value);

        if ( flags & LOOKAT_F_GET )
                print_result(paddr, (unsigned int)vbase, offset);

        if ( flags & LOOKAT_F_GET_MUL )
                print_result_mul(paddr, (unsigned int)vbase, offset, nword);

        if (munmap(vbase, 4096) == -1) {
                perror("munmap failed!");
                exit(EXIT_FAILURE);
        }

        close(fd);

        return 0;
}

