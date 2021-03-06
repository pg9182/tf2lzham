// File: lzham_task_pool_null.h
// See Copyright Notice and license at the end of lzham.h
#pragma once

namespace lzham
{
   class semaphore
   {
      LZHAM_NO_COPY_OR_ASSIGNMENT_OP(semaphore);

   public:
      inline semaphore(long initialCount = 0, long maximumCount = 1, const char* pName = NULL)
      {
         initialCount, maximumCount, pName;
      }

      inline ~semaphore()
      {
      }

      inline void release(long releaseCount = 1, long *pPreviousCount = NULL)
      {
         releaseCount, pPreviousCount;
      }

      inline bool wait(uint32 milliseconds = UINT32_MAX)
      {
         milliseconds;
         return true;
      }      
   };   

   class task_pool
   {
   public:
      inline task_pool() { }
      inline task_pool(uint num_threads) { num_threads; }
      inline ~task_pool() { }

      inline bool init(uint num_threads) { num_threads; return true; }
      inline void deinit();

      inline uint get_num_threads() const { return 0; }
      inline uint get_num_outstanding_tasks() const { return 0; }

      // C-style task callback
      typedef void (*task_callback_func)(uint64 data, void* pData_ptr);
      inline bool queue_task(task_callback_func pFunc, uint64 data = 0, void* pData_ptr = NULL)
      {
         pFunc(data, pData_ptr);
         return true;
      }

      class executable_task
      {
      public:
         virtual void execute_task(uint64 data, void* pData_ptr) = 0;
      };

      // It's the caller's responsibility to delete pObj within the execute_task() method, if needed!
      inline bool queue_task(executable_task* pObj, uint64 data = 0, void* pData_ptr = NULL)
      {
         pObj->execute_task(data, pData_ptr);
         return true;
      }

      template<typename S, typename T>
      inline bool queue_object_task(S* pObject, T pObject_method, uint64 data = 0, void* pData_ptr = NULL)
      {
         (pObject->*pObject_method)(data, pData_ptr);
         return true;
      }

      template<typename S, typename T>
      inline bool queue_multiple_object_tasks(S* pObject, T pObject_method, uint64 first_data, uint num_tasks, void* pData_ptr = NULL)
      {
         for (uint i = 0; i < num_tasks; i++)
         {
            (pObject->*pObject_method)(first_data + i, pData_ptr);
         }
         return true;
      }

      void join() { }
   };
   
   inline void lzham_sleep(unsigned int milliseconds)
   {
      milliseconds;
   }

   inline uint lzham_get_max_helper_threads()
   {
      return 0;
   }

} // namespace lzham
