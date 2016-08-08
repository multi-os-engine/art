/*
 * [XRT] TODO: Copyright header
 */

#ifdef __APPLE__

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE && TARGET_OS_IOS

#include "MachException.h"

#include "fault_handler.h"
#include "thread.h"

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/exception.h>
#include <mach/task.h>

using namespace art;

static mach_port_t new_handler;
static mach_port_t old_handler;
static exception_mask_t old_mask;
static exception_behavior_t old_behavior;
static thread_state_flavor_t old_flavor;

static int xrt_exception_behavior = EXCEPTION_DEFAULT;
static int xrt_exception_mask = EXC_MASK_BAD_ACCESS; // (EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION)

extern "C" boolean_t exc_server(mach_msg_header_t* request, mach_msg_header_t* reply);

#if defined(__arm__)

typedef arm_thread_state32_t xrt_thread_state_t;
typedef arm_exception_state32_t xrt_exception_state_t;
typedef _STRUCT_MCONTEXT32 xrt_struct_mcontext;

#include <mach/arm/thread_status.h>

static thread_state_flavor_t xrt_thread_state_flavor = ARM_THREAD_STATE;
static mach_msg_type_number_t xrt_thread_state_count = ARM_THREAD_STATE_COUNT;

static thread_state_flavor_t xrt_exception_state_flavor = ARM_EXCEPTION_STATE;
static mach_msg_type_number_t xrt_exception_state_count = ARM_EXCEPTION_STATE_COUNT;

#elif defined(__aarch64__)

typedef arm_thread_state64_t xrt_thread_state_t;
typedef arm_exception_state64_t xrt_exception_state_t;
typedef _STRUCT_MCONTEXT64 xrt_struct_mcontext;

#include <mach/arm/thread_status.h>

static thread_state_flavor_t xrt_thread_state_flavor = ARM_THREAD_STATE64;
static mach_msg_type_number_t xrt_thread_state_count = ARM_THREAD_STATE64_COUNT;

static thread_state_flavor_t xrt_exception_state_flavor = ARM_EXCEPTION_STATE64;
static mach_msg_type_number_t xrt_exception_state_count = ARM_EXCEPTION_STATE64_COUNT;

#elif defined(__i386__)

typedef x86_thread_state32_t xrt_thread_state_t;
typedef x86_exception_state32_t xrt_exception_state_t;
typedef _STRUCT_MCONTEXT32 xrt_struct_mcontext;

#include <mach/i386/thread_status.h>

static thread_state_flavor_t xrt_thread_state_flavor = x86_THREAD_STATE32;
static mach_msg_type_number_t xrt_thread_state_count = x86_THREAD_STATE32_COUNT;

static thread_state_flavor_t xrt_exception_state_flavor = x86_EXCEPTION_STATE32;
static mach_msg_type_number_t xrt_exception_state_count = x86_EXCEPTION_STATE32_COUNT;

#elif defined(__x86_64__)

typedef x86_thread_state64_t xrt_thread_state_t;
typedef x86_exception_state64_t xrt_exception_state_t;
typedef _STRUCT_MCONTEXT64 xrt_struct_mcontext;

static thread_state_flavor_t xrt_thread_state_flavor = x86_THREAD_STATE64;
static mach_msg_type_number_t xrt_thread_state_count = x86_THREAD_STATE64_COUNT;

static thread_state_flavor_t xrt_exception_state_flavor = x86_EXCEPTION_STATE64;
static mach_msg_type_number_t xrt_exception_state_count = x86_EXCEPTION_STATE64_COUNT;

#else
#error XTR Mach Exception Handler: Unsupported architecture.
#endif


#define EXC_UNIX_BAD_SYSCALL 0x10000
#define EXC_UNIX_BAD_PIPE    0x10001
#define EXC_UNIX_ABORT       0x10002

int exception_to_signal(exception_type_t type, exception_data_type_t code)
{
    switch (type) {
        case EXC_BAD_ACCESS: {
            if (code == KERN_INVALID_ADDRESS) {
                return SIGSEGV;
            }
            else {
                return SIGBUS;
            }
        }
            
        case EXC_BAD_INSTRUCTION:
            return SIGILL;
            
        case EXC_ARITHMETIC:
            return SIGFPE;
            
        case EXC_EMULATION:
            return SIGEMT;
            
        case EXC_BREAKPOINT:
            return SIGTRAP;
            
        case EXC_SOFTWARE: {
            switch (code) {
                case EXC_UNIX_BAD_SYSCALL:
                    return SIGSYS;
                    
                case EXC_UNIX_BAD_PIPE:
                    return SIGPIPE;
                    
                case EXC_UNIX_ABORT:
                    return SIGABRT;
                    
                case EXC_SOFT_SIGNAL:
                    return SIGKILL;
            }
        }
    }
    return 0;
}

static bool run_exc_server;

extern "C" __attribute__((visibility("default"))) kern_return_t catch_exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task,
                                                                                      exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count) {
    LOG(INFO) << "Caught a Mach exception: port = " << exception_port
        << ", type = " << exception
        << ", code = " << code[0];
    
    kern_return_t result;
    
    xrt_thread_state_t thread_state;
    xrt_exception_state_t exception_state;
    
    xrt_struct_mcontext mctx = {0};
    
    ucontext_t uctx = {0};
    
    uctx.uc_stack.ss_sp = pthread_get_stackaddr_np(pthread_self());
    uctx.uc_stack.ss_flags = 0;
    uctx.uc_stack.ss_size = pthread_get_stacksize_np(pthread_self());
    
    uctx.uc_mcsize = sizeof(mctx);
    uctx.uc_mcontext = &mctx;
    
    result = thread_get_state(thread, xrt_thread_state_flavor, (thread_state_t)&thread_state, &xrt_thread_state_count);
    
    if(result != KERN_SUCCESS) {
        return KERN_FAILURE;
    }
    
    result = thread_get_state(thread, xrt_exception_state_flavor, (thread_state_t)&exception_state, &xrt_exception_state_count);
    
    if(result != KERN_SUCCESS) {
        return KERN_FAILURE;
    }
    
    uctx.uc_mcontext->__ss = thread_state;
    uctx.uc_mcontext->__es = exception_state;
    
    int signal = exception_to_signal(exception, code[0]);
    
    if(!signal) {
        return KERN_FAILURE;
    }
    
    LOG(INFO) << "Converted the Mach exception into a BSD signal: signal = " << signal;
    
    siginfo_t siginfo;
    siginfo.si_signo = signal;
#if defined(__arm__)
    siginfo.si_addr = (void*)uctx.uc_mcontext->__es.__far;
#elif defined(__aarch64__)
    siginfo.si_addr = (void*)uctx.uc_mcontext->__es.__far;
#elif defined(__i386__)
    siginfo.si_addr = (void*)uctx.uc_mcontext->__es.__faultvaddr;
#elif defined(__x86_64__)
    siginfo.si_addr = (void*)uctx.uc_mcontext->__es.__faultvaddr;
#endif
    
    pthread_t thread_target = pthread_from_mach_thread_np(thread);
    Thread* thread_t = Thread::FindThread(thread_target);
    
    int pret = pthread_setspecific(Thread::GetPthreadKey(), thread_t);
    if (pret) {
        return KERN_FAILURE;
    }
    
    if (!fault_manager.HandleFault(signal, &siginfo, &uctx)) {
        mach_port_t self = mach_task_self();
        CHECK(task_set_exception_ports(self, old_mask, old_handler, old_behavior, old_flavor) == KERN_SUCCESS);
        CHECK(mach_port_deallocate(self, new_handler) == KERN_SUCCESS);
        run_exc_server = false;
        
        LOG(INFO) << "Disabled Mach exception handler because of unhandled exception";
    }
    
    pret = pthread_setspecific(Thread::GetPthreadKey(), NULL);
    if (pret) {
        return KERN_FAILURE;
    }
    
    thread_state = uctx.uc_mcontext->__ss;
    result = thread_set_state(thread, xrt_thread_state_flavor, (thread_state_t)&thread_state, xrt_thread_state_count);
    
    return KERN_SUCCESS;
}

static void* exceptionHandlerEntryPoint(void* arg) {
    run_exc_server = true;
    while(run_exc_server) {
        mach_msg_server_once(exc_server, sizeof(mach_msg_base_t) + 1024, new_handler,  0);
    }
    return NULL;
}

bool InstallMachExceptionHandler(void) {
    mach_port_t self = mach_task_self();
    CHECK(mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &new_handler) == KERN_SUCCESS);
    CHECK(mach_port_insert_right(self, new_handler, new_handler, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

    pthread_t thread;
    pthread_attr_t attr;
    CHECK(!pthread_attr_init(&attr));
    CHECK(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
    CHECK(!pthread_create(&thread, &attr, exceptionHandlerEntryPoint, NULL));
    pthread_attr_destroy(&attr);
    
    mach_msg_type_number_t cnt;
    CHECK(task_get_exception_ports(self, EXC_MASK_BAD_ACCESS, &old_mask, &cnt, &old_handler, &old_behavior, &old_flavor) == KERN_SUCCESS);
    
    CHECK(task_set_exception_ports(self, EXC_MASK_BAD_ACCESS, new_handler, EXCEPTION_DEFAULT, MACHINE_THREAD_STATE) == KERN_SUCCESS);
    
    LOG(INFO) << "Created a thread with a Mach exception handler: mask = " << xrt_exception_mask
        << ", port = " << new_handler
        << ", behavior = " << std::hex << std::showbase << xrt_exception_behavior
        << ", flavor = " << std::dec << std::noshowbase << xrt_thread_state_flavor;
    
    return true;
}

#endif /* TARGET_OS_IPHONE && TARGET_OS_IOS */

#endif /* __APPLE__ */
