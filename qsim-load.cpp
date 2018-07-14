/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <fstream>

#include <qsim.h>

#include "qsim-load.h"


using namespace Qsim;
using namespace std;

class QsimLoadHelper {
public:
  QsimLoadHelper(OSDomain &osd, ifstream &infile):
    osd(osd), infile(infile), finished(false)
  {

    // Set the callbacks.
    OSDomain::magic_cb_handle_t magic_handle(
      osd.set_magic_cb(this, &QsimLoadHelper::magic_cb)
    );

    OSDomain::start_cb_handle_t app_start_handle(
      osd.set_app_start_cb(this, &QsimLoadHelper::app_start_cb)
    );
    
    // The main loop: run until 'finished' is true.                            
    while (!finished) {
      osd.run(10000);
      osd.timer_interrupt();
    }

    osd.run(finished_core, 1);

    // Unset the callbacks.
    osd.unset_magic_cb(magic_handle);
    osd.unset_app_start_cb(app_start_handle);
  }

private:
  OSDomain &osd;  
  ifstream &infile;
  bool finished;
  int finished_core;

  int app_start_cb(int c) {
    finished = true;
    finished_core = c;
    osd.set_bench_pid(osd.get_tid(c));
    return 1;
  }

  int magic_cb(int c, uint64_t rax, uint64_t rbx) {

    static int addr_reg, size_reg, ready_reg;

    if (osd.getCpuType(0) == "x86") {
        addr_reg  = QSIM_X86_RBX;
        size_reg  = QSIM_X86_RCX;
        ready_reg = QSIM_X86_RAX;
    } else {
        addr_reg  = QSIM_ARM64_X1;
        size_reg  = QSIM_ARM64_X2;
        ready_reg = QSIM_ARM64_X0;
    }

    if (rax == 0xc5b1fffd) {
      // Giving an address to deposit 1024 bytes in %rbx. Wants number of bytes
      // actually deposited in %rcx.                                           

      uint64_t vaddr = osd.get_reg(c, addr_reg);
      int count = 1024;
      while (infile.good() && count) {
        char ch;
        infile.get(ch);
        osd.mem_wr_virt(c, ch, vaddr++);
        count--;
      }
      osd.set_reg(c, size_reg, 1024-count);
    } else if (rax == 0xc5b1fffe) {
      // Asking if input is ready
      osd.set_reg(c, ready_reg, !(!infile));
    } else if (rax == 0xc5b1ffff) {
      // Asking for a byte of input.
      char ch;
      infile.get(ch);
      osd.set_reg(c, ready_reg, ch);
    } else if ((rax & 0xffffff00) == 0xc5b100) {
      std::cout << "binary write: " << (rax&0xff) << '\n';
    } else if (rax == 0xc5b1fffc) {
      osd.set_n(osd.get_reg(c, addr_reg));
    }

    return 0;
  }
};

void Qsim::load_file(OSDomain &osd, const char *filename) {
  ifstream infile(filename);
  if (infile.fail()) {
	  std::cerr << "Error: Could not open benchmark tar " << filename << std::endl;
	  exit(1);
  }

  QsimLoadHelper qlh(osd, infile);
  infile.close();
}
