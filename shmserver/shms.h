#ifndef DFCONNECT_H
#define DFCONNECT_H

#define SHM_KEY 123466
#define SHM_HEADER 1024 // 1kB reserved for a header
#define SHM_SYNC 512 // second half of the header is reserved for synchronization primitives
#define SHM_BODY 1024*1024 // 1MB reserved for bulk data transfer
#define SHM_SIZE SHM_HEADER+SHM_BODY


// FIXME: add YIELD for linux, add single-core and multi-core compile targets for optimal speed
#ifdef LINUX_BUILD
    // a full memory barrier! better be safe than sorry.
    #include <pthread.h>
    #define full_barrier asm volatile("" ::: "memory"); __sync_synchronize();
    #define SCHED_YIELD sched_yield(); // a requirement for single-core
    
// lawfull evil
struct synchro
{
    pthread_mutex_t mutex;
    pthread_mutexattr_t mattr;
    
    pthread_cond_t cond_set_by_cl;
    pthread_condattr_t clattr;
    
    pthread_cond_t cond_set_by_sv;
    pthread_condattr_t svattr;
};
    
    
    
#else
    // we need windows.h for Sleep()
    #define _WIN32_WINNT 0x0501 // needed for INPUT struct
    #define WINVER 0x0501                   // OpenThread(), PSAPI, Toolhelp32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define SCHED_YIELD Sleep(0); // avoids infinite lockup on single core
    // FIXME: detect MSVC here and use the right barrier magic
    #ifdef __MINGW32__
        #define full_barrier asm volatile("" ::: "memory");
    #else
        #include <intrin.h>
        #pragma intrinsic(_ReadWriteBarrier)
        #define full_barrier _ReadWriteBarrier();
    #endif
#endif

enum DFPP_CmdType
{
    CANCELLATION, // we should jump out of the Act()
    CLIENT_WAIT, // we are waiting for the client
    FUNCTION, // we call a function as a result of the command
};

struct DFPP_command
{
    void (*_function)(void *);
    DFPP_CmdType type:32; // force the enum to 32 bits for compatibility reasons
    std::string name;
    uint32_t nextState;
};

struct DFPP_module
{
    DFPP_module()
    {
        name = "Uninitialized module";
        version = 0;
        modulestate = 0;
    }
    // ALERT: the structures share state
    DFPP_module(const DFPP_module & orig)
    {
        commands = orig.commands;
        name = orig.name;
        modulestate = orig.modulestate;
        version = orig.version;
    }
    inline void set_command(const unsigned int index, const DFPP_CmdType type, const char * name, void (*_function)(void *) = 0,uint32_t nextState = -1)
    {
        commands[index].type = type;
        commands[index].name = name;
        commands[index]._function = _function;
        commands[index].nextState = nextState;
    }
    inline void reserve (unsigned int numcommands)
    {
        commands.clear();
        DFPP_command cmd = {0,CANCELLATION,"",0};
        commands.resize(numcommands,cmd);
    }
    std::string name;
    uint32_t version; // version
    std::vector <DFPP_command> commands;
    void * modulestate;
};

typedef union
{
    struct
    {
        volatile uint16_t command;
        volatile uint16_t module;
    } parts;
    volatile uint32_t pingpong;
    inline void set(uint16_t module, uint16_t command)
    {
        pingpong = module + command << 16;
    }
} shm_cmd;

void SHM_Act (void);
void InitModules (void);
void KillModules (void);
bool isValidSHM();
uint32_t OS_getPID();
DFPP_module InitMaps(void);
uint32_t OS_getAffinity(); // limited to 32 processors. Silly, eh?

#endif
