#ifndef _programtask_h
#define _programtask_h

#include <circle/sched/task.h>

class CProgramTask : public CTask
{
public:
    CProgramTask (void* pProgramTask);
    ~CProgramTask (void);

    void Run (void);
private:
    void* m_pProgramTask(void);
};

#endif