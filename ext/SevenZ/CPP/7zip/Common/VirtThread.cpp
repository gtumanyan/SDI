<<<<<<< HEAD
// VirtThread.cpp

#include "StdAfx.h"

#include "VirtThread.h"

static THREAD_FUNC_DECL CoderThread(void *p)
{
  for (;;)
  {
    CVirtThread *t = (CVirtThread *)p;
    t->StartEvent.Lock();
    if (t->Exit)
      return 0;
    t->Execute();
    t->FinishedEvent.Set();
  }
}

WRes CVirtThread::Create()
{
  RINOK(StartEvent.CreateIfNotCreated());
  RINOK(FinishedEvent.CreateIfNotCreated());
  StartEvent.Reset();
  FinishedEvent.Reset();
  Exit = false;
  if (Thread.IsCreated())
    return S_OK;
  return Thread.Create(CoderThread, this);
}

void CVirtThread::Start()
{
  Exit = false;
  StartEvent.Set();
}

void CVirtThread::WaitThreadFinish()
{
  Exit = true;
  if (StartEvent.IsCreated())
    StartEvent.Set();
  if (Thread.IsCreated())
  {
    Thread.Wait();
    Thread.Close();
  }
}
=======
// VirtThread.cpp

#include "StdAfx.h"

#include "VirtThread.h"

static THREAD_FUNC_DECL CoderThread(void *p)
{
  for (;;)
  {
    CVirtThread *t = (CVirtThread *)p;
    t->StartEvent.Lock();
    if (t->Exit)
      return 0;
    t->Execute();
    t->FinishedEvent.Set();
  }
}

WRes CVirtThread::Create()
{
  RINOK(StartEvent.CreateIfNotCreated());
  RINOK(FinishedEvent.CreateIfNotCreated());
  StartEvent.Reset();
  FinishedEvent.Reset();
  Exit = false;
  if (Thread.IsCreated())
    return S_OK;
  return Thread.Create(CoderThread, this);
}

void CVirtThread::Start()
{
  Exit = false;
  StartEvent.Set();
}

void CVirtThread::WaitThreadFinish()
{
  Exit = true;
  if (StartEvent.IsCreated())
    StartEvent.Set();
  if (Thread.IsCreated())
  {
    Thread.Wait();
    Thread.Close();
  }
}
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
