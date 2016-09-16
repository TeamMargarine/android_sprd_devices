#!/usr/bin/env python
import os
import sys
if sys.hexversion < 0x02070000:
    print >> sys.stderr, "Python 2.7 or newer is required."
    sys.exit(1)
import re
import copy
import shutil
import string
import argparse

############################################################################################
############################### function declaration #######################################
############################################################################################

def make_dir( path ):
    if not os.path.exists(path):
        parent = os.path.dirname(path)
        if parent:
            make_dir(parent)
        os.mkdir(path)

def create_file( relpath ):
    dir = os.path.dirname(relpath )
    make_dir(dir)
    return open(relpath, "w" )

def find_android_root(para_path): 
    path = os.path.realpath(para_path)
    path = os.path.dirname(path)
    
    target_file = "build/envsetup.sh"
    
    while 1:
        if path == "":
            file = target_file
        else:
            file = path + "/" + target_file
        
        if os.path.isfile(file):
            return path
        
        if path == "":
            return None
        
        path = os.path.dirname(path)

    
############################################################################################
############################### class declaration #######################################
#############################################################################################

#############################################################################################

class FileList:

    def __init__(self):
        self.flist = []
        self.ftotal = 0

    def visitfile(self,fname, args): # for each non-dir file
        #print fname
        self.flist.append(fname)
        self.ftotal += 1
         
    def visitor(self,args, directoryName,filesInDirectory): # called for each dir
        for fname in filesInDirectory:                  
            fpath = os.path.join(directoryName, fname)   
            #print fpath
            if not os.path.isdir(fpath):                  
                self.visitfile(fpath,args)
          
    def scan(self,path):
        self.ftotal = 0
        os.path.walk(path, self.visitor, 0)

#############################################################################################

class DeviceFileList:
    re_androidproductmk = re.compile(r".*AndroidProducts.mk")
    re_prodmk = re.compile(r".*prod_(\S+).mk")
    re_boardconfigmk = re.compile(r".*BoardConfig.mk")
    re_vendorsetupsh = re.compile(r".*vendorsetup.sh")
    
    def __init__(self):
        self.device_name = ""
        self.device_path = ""
        self.full_list = []
        self.board_config_mk = ""
        self.vendor_setup_sh = ""
        self.prod_mk_list = []
        self.other_list = []
        self.kernel_defconfig_list = []
        self.uboot_defconfig_list = []

    def find_boardconfigmk(self):
        for f in self.full_list:
            m = DeviceFileList.re_boardconfigmk.match(f)
            if m:
                self.board_config_mk = m.group()
                self.other_list.remove(self.board_config_mk)
                break
            
    def find_vendorsetupsh(self):
        for f in self.full_list:
            m = DeviceFileList.re_vendorsetupsh.match(f)
            if m:
                self.vendor_setup_sh = m.group()
                self.other_list.remove(self.vendor_setup_sh)
                break
            
    def find_prodmk(self):
        for f in self.full_list:
            m = DeviceFileList.re_prodmk.match(f)
            if m:
                str = m.group()
                self.prod_mk_list.append(str)
                self.other_list.remove(str)
            
    def process(self,para_name,para_devicetop):
        self.device_name = para_name 
        self.device_path = para_devicetop + para_name 
        mlist = FileList()
        mlist.scan(self.device_path)
        self.full_list = mlist.flist
        self.other_list = copy.deepcopy(self.full_list)
        self.find_boardconfigmk()
        self.find_vendorsetupsh()
        self.find_prodmk()
        


#############################################################################################
class DeviceFork:
    def __init__(self):
        self.ori_name = ""
        self.tgt_name = ""
        self.udef_cnt = 0
        
    def set_name(self,para_oname,para_tname):
        self.ori_name = para_oname
        self.tgt_name = para_tname
    
    def gen_prodmk_getline(self,line):
        pos = line.find(self.ori_name)
        if pos < 0:
            return line
        else:
            line = line.replace(self.ori_name,self.tgt_name)
            return line
    
    def gen_prodmk(self,para_ori_file,para_tgt_file):
        
        ori_file = para_ori_file
        tgt_file = para_tgt_file
        content_buf = []
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_prodmk_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            f_tgt = create_file(tgt_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    
    def gen_prodmks(self,para_prodmklst):
        re_ori_name = re.compile(r"(.*/)(%s)(/.*)" % self.ori_name)
        prodmklist = para_prodmklst
        for f in prodmklist:
            m = re_ori_name.match(f)
            if m:
                nf = m.group(1) + self.tgt_name + m.group(3)
                self.gen_prodmk(f,nf)
                print "Add:" + nf 
        
    def gen_vendorsetupsh_getline(self,line):
        pos = line.find(self.ori_name)
        if pos < 0:
            return line
        else:
            line = line.replace(self.ori_name,self.tgt_name)
            return line
    
    def gen_vendorsetupsh_getfile(self,para_ori_file,para_tgt_file):
        
        ori_file = para_ori_file
        tgt_file = para_tgt_file
        content_buf = []
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_vendorsetupsh_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            f_tgt = create_file(tgt_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_vendorsetupsh(self,para_vendorsetupsh):
        re_ori_name = re.compile(r"(.*/)(%s)(/.*)" % self.ori_name)
        ori_vendorsetupsh = para_vendorsetupsh
        m = re_ori_name.match(ori_vendorsetupsh)
        if m:
            nf = m.group(1) + self.tgt_name + m.group(3)
            self.gen_vendorsetupsh_getfile(ori_vendorsetupsh,nf)
            print "Add:" + nf

    def gen_boardconfigmk_getline(self,line):
        re_ub_def = re.compile(r"(.*)(UBOOT_DEFCONFIG)(.*)")
        m = re_ub_def.match(line)
        if m:
            if self.udef_cnt == 0:
                str = "UBOOT_DEFCONFIG := " + self.tgt_name + "\n"
                self.udef_cnt = self.udef_cnt + 1
                return str
            
            if self.udef_cnt == 1:
                str = "UBOOT_DEFCONFIG := " + self.tgt_name + "_native" + "\n"
                self.udef_cnt = self.udef_cnt + 1
                return str

        else:
            pos = line.find(self.ori_name)
            if pos < 0:
                return line
            else:
                line = line.replace(self.ori_name,self.tgt_name)
                return line
    
    def gen_boardconfigmk_getfile(self,para_ori_file,para_tgt_file):
        
        ori_file = para_ori_file
        tgt_file = para_tgt_file
        content_buf = []
        self.udef_cnt = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_boardconfigmk_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            f_tgt = create_file(tgt_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_boardconfigmk(self,para_boardconfigmk):
        re_ori_name = re.compile(r"(.*/)(%s)(/.*)" % self.ori_name)
        ori_boardconfigmk = para_boardconfigmk
        m = re_ori_name.match(ori_boardconfigmk)
        if m:
            nf = m.group(1) + self.tgt_name + m.group(3)
            self.gen_boardconfigmk_getfile(ori_boardconfigmk,nf)
            print "Add:" + nf
            
    def gen_other(self,para_otherlst):
        re_ori_name = re.compile(r"(.*/)(%s)(/.*)" % self.ori_name)
        otherlist = para_otherlst
        for f in otherlist:
            m = re_ori_name.match(f)
            if m:
                nf = m.group(1) + self.tgt_name + m.group(3)
                dir = os.path.dirname(nf)
                make_dir(dir)
                shutil.copy(f,nf)
                print "Add:" + nf
        
#############################################################################################

class BoardConfigParser:
    re_k_defconfig = re.compile(r"KERNEL_DEFCONFIG(.*=)(\s*)(.*)$")
    re_u_defconfig = re.compile(r"UBOOT_DEFCONFIG(.*=)(\s*)(.*)$")
    
    def __init__(self):
        self.k_defconfig_list = []
        self.u_defconfig_list = []
        self.defconfig_top = ""

    def parseLine(self,line):
        line = string.strip(line)

        # skip empty and comment lines
        if len(line) == 0 or line[0] == "#":
            return

        mk = BoardConfigParser.re_k_defconfig.match(line)
        if mk:
            self.k_defconfig_list.append(self.defconfig_top + mk.group(3))
            return
        
        mu = BoardConfigParser.re_u_defconfig.match(line)
        if mu:
            self.u_defconfig_list.append(mu.group(3))
            return
            
    def parseFile(self,path,para_kernel_top):
        self.defconfig_top = para_kernel_top + "arch/arm/configs/"
        f = file(path, "r")
        for line in f:
            if len(line) > 0:
                if line[-1] == "\n":
                    line = line[:-1]
                    if len(line) > 0 and line[-1] == "\r":
                        line = line[:-1]
                self.parseLine(line)
        f.close()
        
#############################################################################################

class KernelDefconfigParser:
    re_mach = re.compile(r"^CONFIG_ARCH_(?!HAS)(\S*)(.*=)(.*)$")
    re_board = re.compile(r"^CONFIG_MACH_(\S*)(.*=)(.*)$")

    def __init__(self):
        self.mach_name = []
        self.board_name = []
        self.mach_found = 0
        self.board_found = 0

    def parseLine(self,line):
        line = string.strip(line)

        # skip empty and comment lines
        if len(line) == 0 or line[0] == "#":
            return

        if self.mach_found == 0:
            mm = KernelDefconfigParser.re_mach.match(line)
            if mm:
                self.mach_name = string.lower(mm.group(1))
                self.mach_found = 1
                return
        
        if self.board_found == 0:
            mb = KernelDefconfigParser.re_board.match(line)
            if mb:
                self.board_name = string.lower(mb.group(1))
                self.board_found = 1
                return
            

    def parseFile(self,path):
        f = file(path, "r")
        for line in f:
            if len(line) > 0:
                if line[-1] == "\n":
                    line = line[:-1]
                    if len(line) > 0 and line[-1] == "\r":
                        line = line[:-1]
                self.parseLine(line)
        f.close()

#############################################################################################

class BoardHeaderPaser:
    re_gpio_header = re.compile(r"(.*)<mach/(\S*)>(.*)")

    def __init__(self):
        self.gpio_header = []
        self.board_name = ""
        self.state = 0

    def parseLine(self,line):

        if self.state == 0:
            str = string.upper(self.board_name)
            re_config_board = re.compile(r"(.*)CONFIG_MACH_(%s)(\s+|\/.*)" % str )
            m = re_config_board.match(line)
            if m:
                return 1
        
        if self.state == 1:
            m = BoardHeaderPaser.re_gpio_header.match(line)
            if m:
                self.gpio_header = m.group(2)
                return 2
        
        return 0
                

    def parseFile(self,path,para_boardname):
        self.board_name = para_boardname
        f = file(path, "r")
        for line in f:
            if len(line) > 0:
                self.state = self.parseLine(line)
                if self.state == 2:
                    break
        f.close()
        
#############################################################################################

class KernelMachMakefileParser:

    def __init__(self):
        self.gpio_header = []
        self.board_name = ""
        self.board_folder = ""
        self.state = 0

    def parseLine(self,line):
        line = string.strip(line)
        
        str = string.upper(self.board_name)
        re_config_board = re.compile(r"sprdboard(.*)CONFIG_MACH_(%s)\).*\+=(\s*)(\S+)(/.*)" % str )
        m = re_config_board.match(line)
        if m:
            self.board_folder = m.group(4)
            return 1
        
        return 0
                

    def parseFile(self,path,para_boardname):
        self.board_name = para_boardname
        f = file(path, "r")
        for line in f:
            if len(line) > 0:
                self.state = self.parseLine(line)
                if self.state == 1:
                    break
        f.close()
        
#############################################################################################
class KernelFileList:
    re_androidproductmk = re.compile(r".*AndroidProducts.mk")
    re_prodmk = re.compile(r".*prod_(\S+).mk")
    re_boardconfigmk = re.compile(r".*BoardConfig.mk")
    re_vendorsetupsh = re.compile(r".*vendorsetup.sh")
    
    def __init__(self):
        self.board_name = ""
        self.kernel_top = ""
        self.mach_name = ""
        self.mach_path = ""
        self.board_path = ""
        self.kernel_defconfig_list = []
        self.board_file_list = []
        self.board_header = ""
        self.gpio_header = ""
        self.mach_makefile = ""
        self.mach_kconfig = ""


    def find_gpio_header(self):
        bhparser = BoardHeaderPaser()
        bhparser.parseFile(self.board_header,self.board_name)
        self.gpio_header = self.mach_path + "/include/mach/" + bhparser.gpio_header
        
    def find_boardfiles(self):
        mlist = FileList()
        mlist.scan(self.board_path)
        self.board_file_list = mlist.flist

    def find_mach_board(self):
        cparser = KernelDefconfigParser()
        cparser.parseFile(self.kernel_defconfig_list[0])
        self.mach_name = cparser.mach_name
        self.board_name = cparser.board_name
        self.mach_path = self.kernel_top + "arch/arm/mach-" + self.mach_name
#        self.board_path = self.mach_path + "/board_" + self.board_name
        self.mach_makefile = self.mach_path + "/Makefile"
        self.mach_kconfig = self.mach_path + "/Kconfig"
        self.board_header = self.mach_path + "/include/mach/board.h"
        
        kmmparser = KernelMachMakefileParser()
        kmmparser.parseFile(self.mach_makefile,self.board_name)
        
        self.board_path = self.mach_path + "/" + kmmparser.board_folder
        
        
            
    def set_defconfig_list(self,para_defconfig_list):
        self.kernel_defconfig_list = copy.deepcopy(para_defconfig_list)
            
    def process(self,para_name,para_kerneltop):
        self.board_name = para_name 
        self.kernel_top = para_kerneltop
        
        self.find_mach_board()
        self.find_boardfiles()
        self.find_gpio_header()
        
#############################################################################################


class KernelFork:
    def __init__(self):
        self.ori_name = ""
        self.tgt_name = ""
        self.mach_name = ""
        self.mach_makefile_found = 0
        self.board_header_found = 0
        self.kdc_board_found = 0
        self.mach_kconfig_found = 0
    
    def set_name(self,para_oname,para_tname,para_mach_name):
        self.ori_name = para_oname
        self.tgt_name = para_tname
        self.mach_name = para_mach_name

    def gen_board_header_getline(self,line):
        re_board_header = re.compile(r"#ifdef.*CONFIG_MACH_.*")
        if self.board_header_found == 0:
            m = re_board_header.match(line)
            if m:
                self.board_header_found = 1
                str1 = "#ifdef CONFIG_MACH_%s" % self.tgt_name.upper()
                str2 = "#include <mach/gpio-%s.h>" % self.tgt_name
                str3 = "#endif"
                line = str1 + "\n" + str2 + "\n" + str3 + "\n" + line
                return line
            else:
                return line
        else:
            return line
        
    def gen_board_header_getfile(self,para_ori_file):
        ori_file = para_ori_file
        tgt_file = para_ori_file + ".bak"
        print "Backup:" + ori_file + " to: " + tgt_file

        content_buf = []
        self.board_header_found = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_board_header_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            shutil.copy(ori_file,tgt_file)
                
            f_tgt = create_file(ori_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_board_header(self,para_board_header):
        board_header = para_board_header
        self.gen_board_header_getfile(board_header)
        print "Modify:" + board_header
        
    def gen_mach_makefile_getline(self,line):
        re_mach_makefile = re.compile(r"sprdboard.*")
        if self.mach_makefile_found == 0:
            m = re_mach_makefile.match(line)
            if m:
                self.mach_makefile_found = 1
                str = "sprdboard-$(CONFIG_MACH_%s) += board_%s/" % (self.tgt_name.upper(),self.tgt_name)
                line = str + "\n" + line
                return line
            else:
                return line
        else:
            return line
    
    def gen_mach_makefile_getfile(self,para_ori_file):
        
        ori_file = para_ori_file
        tgt_file = para_ori_file + ".bak"
        print "Backup:" + ori_file + " to: " + tgt_file

        content_buf = []
        self.mach_makefile_found = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_mach_makefile_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            shutil.copy(ori_file,tgt_file)
                
            f_tgt = create_file(ori_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_mach_makefile(self,para_mach_makefile):
        ori_mach_makefile = para_mach_makefile
        self.gen_mach_makefile_getfile(ori_mach_makefile)
        print "Modify:" + ori_mach_makefile
    
    
    def gen_mach_kconfig_getline(self,line):
        re_mach_kconfig = re.compile(r"(.*)config(\s*)MACH_(%s)(.*)" % self.ori_name.upper())
        if self.mach_kconfig_found == 0:
            m = re_mach_kconfig.match(line)
            if m:
                self.mach_kconfig_found = 1
                str1 = "config MACH_%s\n" % self.tgt_name.upper()
                str2 = "\tbool \"%s\"\n" % self.tgt_name.upper()
                str3 = "\thelp\n"
                str4 = "\t  %s based on %s.\n" % (self.tgt_name.upper(),self.mach_name.upper()) 
                line = str1 + str2 + str3 + str4 + line
                return line
            else:
                return line
        else:
            return line
    
    def gen_mach_kconfig_getfile(self,para_ori_file):
        
        ori_file = para_ori_file
        tgt_file = para_ori_file + ".bak"
        print "Backup:" + ori_file + " to: " + tgt_file

        content_buf = []
        self.mach_kconfig_found = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_mach_kconfig_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            shutil.copy(ori_file,tgt_file)
                
            f_tgt = create_file(ori_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_mach_kconfig(self,para_mach_kconfig):
        ori_mach_kconfig = para_mach_kconfig
        self.gen_mach_kconfig_getfile(ori_mach_kconfig)
        print "Modify:" + ori_mach_kconfig
        
    def gen_board_files(self,para_boardlst):
        re_ori_name = re.compile(r"(.*)(board\S+)(%s)(/.*)" % self.ori_name)
        boardfiles = para_boardlst
        for f in boardfiles:
            m = re_ori_name.match(f)
            if m:
                nf = m.group(1) + "board_" + self.tgt_name + m.group(4)
                dir = os.path.dirname(nf)
                make_dir(dir)
                shutil.copy(f,nf)
                print "Add:" + nf
                
    def gen_gpio_header(self,para_gpio_header):
        re_ori_name = re.compile(r"(.*)(gpio-)(%s)(\.h.*)" % self.ori_name)
        gpio_header = para_gpio_header
        m = re_ori_name.match(gpio_header)
        if m:
            nf = m.group(1) + m.group(2) + self.tgt_name + m.group(4)
            dir = os.path.dirname(nf)
            make_dir(dir)
            shutil.copy(gpio_header,nf)
            print "Add:" + nf
            
    def gen_kernel_defconfig_getline(self,line):
        re_kdc_board = re.compile(r"^CONFIG_MACH_(\S*)(.*=)(.*)$")
        if self.kdc_board_found == 0:
            m = re_kdc_board.match(line)
            if m:
                self.kdc_board_found = 1
                str1 = "CONFIG_MACH_%s=y" % self.tgt_name.upper()
                str2 = "# CONFIG_MACH_%s is not set" % m.group(1)
                line = str1 + "\n" + str2 + "\n"
                return line
            else:
                return line
        else:
            return line
    
    def gen_kernel_defconfig(self,para_ori_file,para_tgt_file):
        
        ori_file = para_ori_file
        tgt_file = para_tgt_file
        content_buf = []
        
        self.kdc_board_found = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_kernel_defconfig_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            f_tgt = create_file(tgt_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )

        
    
    def gen_kernel_defconfigs(self,para_kdefconfiglst):
        re_ori_name = re.compile(r"(.*/)(%s)(-.*)" % self.ori_name)
        kdefconfiglist = para_kdefconfiglst
        for f in kdefconfiglist:
            m = re_ori_name.match(f)
            if m:
                nf = m.group(1) + self.tgt_name + m.group(3)
                self.gen_kernel_defconfig(f,nf)
                print "Add:" + nf
                
#############################################################################################
#############################################################################################

class UbootMakefileParser:

    def __init__(self):
        self.udef_name = ""
        self.soc_name = ""
        self.board_name = ""
        self.state = 0

    def parseLine(self,line):
        line = string.strip(line)

        if self.state == 0:
            re_config_udef = re.compile(r"(%s)_config(.*)" % self.udef_name )
            m = re_config_udef.match(line)
            if m:
                return 1
        
        if self.state > 0:
            re_soc = re.compile(r"(.*)(\s+)(\S+)(\s+)spreadtrum(\s+)(\S+)")
            m = re_soc.match(line)
            if m:
                self.soc_name = m.group(6)
                self.board_name = m.group(3)
                return 5
            else:
                return self.state + 1
        
        return 0
                

    def parseFile(self,path,para_udef_name):
        self.udef_name = para_udef_name
        f = file(path, "r")
        for line in f:
            if len(line) > 0:
                self.state = self.parseLine(line)
                if self.state == 5:
                    break
        f.close()
################################################################################
class UbootFileList:
    def __init__(self):
        self.board_name = ""
        self.device_name = ""
        self.soc_name = ""
        self.uboot_top = ""
        self.uboot_defconfig_list = []
        self.board_file_list = []
        self.board_header_list = []
        self.uboot_makefile = ""
    
    def set_board_header_list(self):
        for name in self.uboot_defconfig_list:
            lpath = "/include/configs/%s.h" % name
            path = self.uboot_top + lpath
            self.board_header_list.append(path)
        
    def find_boardfiles(self):
        lpath = "board/spreadtrum/%s/" % self.board_name
        path = self.uboot_top + lpath
        mlist = FileList()
        mlist.scan(path)
        self.board_file_list = mlist.flist
            
    def set_defconfig_list(self,para_defconfig_list):
        self.uboot_defconfig_list = copy.deepcopy(para_defconfig_list)
        
    def find_soc_name(self,para_uboot_makefile):
        uboot_makefile = para_uboot_makefile
        umparser = UbootMakefileParser()
        umparser.parseFile(uboot_makefile,self.uboot_defconfig_list[0])
        self.soc_name = umparser.soc_name
        self.board_name = umparser.board_name
        
    def process(self,para_name,para_uboottop):
        self.device_name = para_name
        
        self.uboot_top = para_uboottop
        self.uboot_makefile = self.uboot_top + "/Makefile"
        
        self.find_soc_name(self.uboot_makefile)
        
        self.set_board_header_list()
        self.find_boardfiles()
        
        
#############################################################################################


class UbootFork:
    def __init__(self):
        self.ori_udef_name = ""
        self.ori_board_name = ""
        self.tgt_name = ""
        self.soc_name = ""
        self.makefile_found = 0
    
    def set_name(self,para_oname_udef,para_oname_board,para_tname):
        self.ori_udef_name = para_oname_udef
        self.ori_board_name = para_oname_board
        self.tgt_name = para_tname
        
    def gen_board_header(self,para_board_header_lst):
        re_ori_name = re.compile(r"(.*/)(%s)(.h|_native.h)" % self.ori_udef_name)
        board_head_files = para_board_header_lst
        for f in board_head_files:
            m = re_ori_name.match(f)
            if m:
                if os.path.exists(f):
                    nf = m.group(1) + self.tgt_name + m.group(3)
                    dir = os.path.dirname(nf)
                    make_dir(dir)
                    shutil.copy(f,nf)
                    print "Add:" + nf
                else:
                    sys.stderr.write( "error: file '%s' does not exist\n" % f )
                
    def gen_makefile_getline(self,line):
        re_makefile = re.compile(r"(%s)_config(.*)" % self.ori_udef_name )
        if self.makefile_found == 0:
            m = re_makefile.match(line)
            if m:
                self.re_makefile = 1
                str1 = "%s_config : unconfig\n" % self.tgt_name
                str2 = "%s_native_config  : unconfig\n" % self.tgt_name
                str3 = "\t@mkdir -p $(obj)include\n"
                str4 = "\t@echo \"CONFIG_NAND_U_BOOT = y\" >> $(obj)include/config.mk\n"
                str5 = "\t@$(MKCONFIG) $@ arm armv7 %s spreadtrum %s\n" % (self.tgt_name,self.soc_name)
                str = str1 + str3 + str4 + str5 + "\n"
                str = str + str2 + str3 + str4 + str5 + "\n"
                line = str + line
                return line
            else:
                return line
        else:
            return line
    
    def gen_makefile_getfile(self,para_ori_file):
        
        ori_file = para_ori_file
        tgt_file = para_ori_file + ".bak"
        print "Backup:" + ori_file + " to: " + tgt_file
        
        content_buf = []
        self.makefile_found = 0
        
        if os.path.exists(ori_file):
            f_orig = file(ori_file,"r")
            for line in f_orig:
                str = self.gen_makefile_getline(line)
                content_buf.append(str)
            f_orig.close()
        
            shutil.copy(ori_file,tgt_file)
                
            f_tgt = create_file(ori_file)
            f_tgt.writelines(content_buf)
            f_tgt.close()
        else:
            sys.stderr.write( "error: file '%s' does not exist\n" % ori_file )
        
    def gen_makefile(self,para_makefile,para_soc_name):
        ori_makefile = para_makefile
        self.soc_name = para_soc_name
        self.gen_makefile_getfile(ori_makefile)
        print "Modify:" + ori_makefile
            
    def gen_board_files(self,para_boardlst):
        re_ori_name = re.compile(r"(.*/)(%s)(/.*)" % self.ori_board_name)
        boardfiles = para_boardlst
        for f in boardfiles:
            m = re_ori_name.match(f)
            if m:
                nf = m.group(1) + self.tgt_name + m.group(3)
                dir = os.path.dirname(nf)
                make_dir(dir)
                shutil.copy(f,nf)
                print "Add:" + nf
                        
#############################################################################################



############################################################################################
############################### flow declaration #######################################
############################################################################################
androidtop = find_android_root(sys.argv[0])
if not androidtop:
    print "could not find the android top directory. exit ..."
    sys.exit(1)

if androidtop[-1] != '/':
    androidtop += "/" 

devicetop = androidtop + "/device/sprd/"
kerneltop = androidtop + "/kernel/"
uboottop  = androidtop + "/u-boot/"

BaseProject = os.listdir(devicetop)

if ".git" in BaseProject:
    BaseProject.remove(".git")
    
if "common" in BaseProject:
    BaseProject.remove("common")
    
if "partner" in BaseProject:
    BaseProject.remove("partner")

parser = argparse.ArgumentParser(description="This is Spreadtrum Android Project Fork Script.")

parser.add_argument('-c',nargs=2,help='fork form base_project to target_project',metavar=('base', 'target'))
parser.add_argument('-i',help='fork process interactively',action='store_true')
parser.add_argument('-l',help='list current project',action='store_true')

args = parser.parse_args()

if args.l:
    print "You have these projects right now: \n %s\n" % BaseProject
    sys.exit(1)

if args.i:
    
    print "You have these projects right now: \n %s\n" % BaseProject

    choice = raw_input("please enter the project name you want base on:")

    if choice in BaseProject:
        print "OK"
    else:
        print "your choice (%s) is not exist,exit ... " % choice
        sys.exit(1)

    choice2 = raw_input("please enter the project name you want to fork:\n")

    if choice2 in BaseProject:
        print "This project exists already,exit ..."
        sys.exit(1)

if args.c == None:
    print "C None"
else:
    choice = args.c[0]
    choice2 = args.c[1]
    if choice in BaseProject:
        print "OK"
    else:
        print "base project (%s) is not exist,exit ... " % choice
        sys.exit(1)
    
    if choice2 in BaseProject:
        print "target project exists already,exit ..."
        sys.exit(1)
    
choice2_pos = choice2.find("-")
if choice2_pos < 0:
    print "OK"
else:
    print "Please use \"_\" instead of \"-\"\n"
    sys.exit(1)
        
# process device files

device_file_list = DeviceFileList()
device_file_list.process(choice,devicetop)

device_forker = DeviceFork()
device_forker.set_name(choice,choice2)
device_forker.gen_prodmks(device_file_list.prod_mk_list)
device_forker.gen_vendorsetupsh(device_file_list.vendor_setup_sh)
device_forker.gen_boardconfigmk(device_file_list.board_config_mk)
device_forker.gen_other(device_file_list.other_list)

# get boardconfigmk info
boardconfigmk_info = BoardConfigParser()
boardconfigmk_info.parseFile(device_file_list.board_config_mk,kerneltop)

# process kernel files
kernel_file_list = KernelFileList()
kernel_file_list.set_defconfig_list(boardconfigmk_info.k_defconfig_list)
kernel_file_list.process(choice,kerneltop)

kernel_forker = KernelFork()
kernel_forker.set_name(choice,choice2,kernel_file_list.mach_name)
kernel_forker.gen_board_files(kernel_file_list.board_file_list)
kernel_forker.gen_gpio_header(kernel_file_list.gpio_header)
kernel_forker.gen_mach_makefile_getfile(kernel_file_list.mach_makefile)
kernel_forker.gen_board_header(kernel_file_list.board_header)
kernel_forker.gen_kernel_defconfigs(kernel_file_list.kernel_defconfig_list)
kernel_forker.gen_mach_kconfig(kernel_file_list.mach_kconfig)

# process u-boot files
uboot_file_list = UbootFileList()
uboot_file_list.set_defconfig_list(boardconfigmk_info.u_defconfig_list)
uboot_file_list.process(choice,uboottop)

uboot_forker = UbootFork()
uboot_forker.set_name(boardconfigmk_info.u_defconfig_list[0],uboot_file_list.board_name,choice2)
uboot_forker.gen_board_files(uboot_file_list.board_file_list)
uboot_forker.gen_board_header(uboot_file_list.board_header_list)
uboot_forker.gen_makefile(uboot_file_list.uboot_makefile,uboot_file_list.soc_name)

print "This Work Done!"
sys.exit(0)



