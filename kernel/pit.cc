#include "pit.h"
#include "debug.h"
#include "machine.h"
#include "idt.h"
#include "smp.h"
#include "threads.h"


// TODO: WTF IS THIS 
constexpr double PI = 3.14159265358979323846;
int position = 0;

/* The standard frequency of the PIT */
constexpr uint32_t PIT_FREQ = 1193182;

/* Were we want the APIT to iunterrupt us */
constexpr uint32_t APIT_vector = 40; 

uint32_t Pit::jiffiesPerSecond = 0;
uint32_t Pit::apitCounter = 0;
uint32_t Pit::jiffies = 0;

struct PitInfo {
};

static PitInfo *pitInfo = nullptr;

/* Do what you need to do in order to run the APIT at the given
 * frequency. Should be called by the bootstrap CPI
 */
void Pit::calibrate(uint32_t hz) {

    pitInfo = new PitInfo();
    

    SMP::apit_lvt_timer.set(0x00010000); // oneshot, masked, ...
    SMP::apit_divide.set(0x0000000B); // divide by 1

    // Now let's program the PIT to compute the frequency
    Debug::printf("| pitInit freq %dHz\n",hz);
    uint32_t d = PIT_FREQ / 20;

    if ((d & 0xffff) != d) {
        Debug::printf("| pitInit invalid divider %d\n",d);
        d = 0xffff;
    }
    Debug::printf("| pitInit divider %d\n",d);

    uint32_t initial = 0xffffffff;
    SMP::apit_initial_count.set(initial);

    outb(0x61,1);          // speaker off, gate on

    outb(0x43,0b10110110); //  10 -> channel#2
                           //  11 -> lobyte/hibyte
                           // 011 -> square wave generator
                           //   0 -> count in binary


    // write the divider to the PIT
    outb(0x42,d);
    outb(0x42,d >> 8);

    uint32_t last = inb(0x61) & 0x20;
    uint32_t changes = 0;
    // The PIT counts twice as fast when it runs in the
    // square-wave generator mode. So, the state is
    // really changing at 40Hz and we should loop
    // for 40 iterations if we want to wait for a second
    // 
    while(changes < 40) {
        uint32_t t = inb(0x61) & 0x20;
        if (t != last) {
            changes ++;
            last = t;
        }
    }
    
    uint32_t diff = initial - SMP::apit_current_count.get();

    // stop the PIT
    outb(0x61,0);

    Debug::printf("| APIT running at %uHz\n",diff);
    apitCounter = diff / hz;
    jiffiesPerSecond = hz;
    Debug::printf("| APIT counter=%d for %dHz\n",apitCounter,hz);

    // Register the APIT interrupt handler
    IDT::interrupt(APIT_vector, (uint32_t)apitHandler_);
}

// Called by each CPU in order to initialize its own PIT
void Pit::init() {
    if (apitCounter == 0) {
        Debug::panic("apiCounter == 0, did you call Pit::calibrate?\n");
    }

    SMP::apit_divide.set(0x0000000B); // divide by 1

    // The following line will enable timer interrupts for this CPU
    // You better be prepared for it
    SMP::apit_lvt_timer.set(
        (1 << 17) |      // Timer mode: 1 -> Periodic
        0 << 16   |      // mask: 0 -> interrupts not masked
        APIT_vector      // the interrupt vector
    );
        
    // Let's go
    SMP::apit_initial_count.set(apitCounter);
}




double sin(double x) {
  double sum = 0.0;
  double term = x;
  double numerator = x;
  double denominator = 1.0;
  int sign = 1;
  for (int i = 0; i < 10; i++) { // compute 10 terms of the Taylor series
    sum += sign * term;
    numerator *= x * x;
    denominator *= (2 * i + 2) * (2 * i + 3);
    term = numerator / denominator;
    sign = -sign;
  }
  return sum;
}


extern "C" void apitHandler(uint32_t* things) {
  auto id = SMP::me();
  if (id == 0) {
    Pit::jiffies ++;
  }
  SMP::eoi_reg.set(0);
  auto me = gheith::activeThreads[id];
  if ((me == nullptr) || (me->isIdle) || (me->saveArea.no_preempt)) return;

  // update the speaker position with a sine wave
  constexpr int frequency = 440;
  constexpr int amplitude = 127;
  constexpr int sampleRate = 44100;
  const double phase = 2.0 * PI * frequency / sampleRate;
  const int sample = static_cast<int>(amplitude * sin(position * phase));
  position = (position + 1) % sampleRate;
  outb(0x61, sample | 0x3); // set speaker position and turn speaker on

  yield();
}



// extern "C" void apitHandler(uint32_t* things) {
//     // interrupts are disabled.
//     auto id = SMP::me();
//     if (id == 0) {
//         Pit::jiffies ++;
//     }
//     SMP::eoi_reg.set(0);
//     auto me = gheith::activeThreads[id];
//     if ((me == nullptr) || (me->isIdle) || (me->saveArea.no_preempt)) return;
//     yield();
// }