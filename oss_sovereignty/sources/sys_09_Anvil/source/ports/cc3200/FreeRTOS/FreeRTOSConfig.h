

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H



#define configUSE_PREEMPTION			        1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK				        1
#define configCPU_CLOCK_HZ				        ( ( unsigned long ) 80000000 )
#define configTICK_RATE_HZ				        ( ( TickType_t ) 1000 )
#define configMINIMAL_STACK_SIZE		        ( ( unsigned short ) 72 )
#define configTOTAL_HEAP_SIZE			        ( ( size_t ) ( \
    16384  \
    - sizeof(StaticTask_t) - configMINIMAL_STACK_SIZE * sizeof(StackType_t)  \
    - sizeof(StaticTask_t) - 1024  \
    - sizeof(StaticTask_t) - 6656  \
    - sizeof(StaticTask_t) - 896  \
    ) )
#define configMAX_TASK_NAME_LEN			        ( 8 )
#define configUSE_TRACE_FACILITY		        0
#define configUSE_16_BIT_TICKS			        0
#define configIDLE_SHOULD_YIELD			        1
#define configUSE_CO_ROUTINES 			        0
#define configUSE_MUTEXES				        0
#define configUSE_RECURSIVE_MUTEXES		        0
#ifdef DEBUG
#define configCHECK_FOR_STACK_OVERFLOW	        1
#else
#define configCHECK_FOR_STACK_OVERFLOW          0
#endif
#define configUSE_QUEUE_SETS			        0
#define configUSE_COUNTING_SEMAPHORES	        0
#define configUSE_ALTERNATIVE_API		        0

#define configMAX_PRIORITIES			        ( 4UL )
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )
#define configQUEUE_REGISTRY_SIZE		        0


#define configUSE_TIMERS				        0
#define configTIMER_TASK_PRIORITY		        2
#define configTIMER_QUEUE_LENGTH		        20
#define configTIMER_TASK_STACK_DEPTH	        ( configMINIMAL_STACK_SIZE * 2 )
#ifdef DEBUG
#define configUSE_MALLOC_FAILED_HOOK            1
#else
#define configUSE_MALLOC_FAILED_HOOK            0
#endif
#define configENABLE_BACKWARD_COMPATIBILITY     0


#define INCLUDE_vTaskPrioritySet				0
#define INCLUDE_uxTaskPriorityGet				0
#define INCLUDE_vTaskDelete						0
#define INCLUDE_vTaskCleanUpResources			0
#define INCLUDE_vTaskSuspend					0
#define INCLUDE_vTaskDelayUntil					0
#define INCLUDE_vTaskDelay						1
#ifdef DEBUG
#define INCLUDE_uxTaskGetStackHighWaterMark		1
#else
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#endif
#define INCLUDE_xTaskGetSchedulerState			0
#define INCLUDE_xTimerGetTimerDaemonTaskHandle	0
#ifdef DEBUG
#define INCLUDE_xTaskGetIdleTaskHandle			1
#else
#define INCLUDE_xTaskGetIdleTaskHandle          0
#endif
#define INCLUDE_pcTaskGetTaskName				0
#define INCLUDE_eTaskGetState					0
#define INCLUDE_xSemaphoreGetMutexHolder		0

#define configKERNEL_INTERRUPT_PRIORITY 		( 7 << 5 )	

#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( 1 << 5 )  


#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1


#define configAPPLICATION_ALLOCATED_HEAP 1


#define configSUPPORT_STATIC_ALLOCATION 1


#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1
#undef configUSE_MUTEXES
#define configUSE_MUTEXES 1
#undef INCLUDE_vTaskDelete
#define INCLUDE_vTaskDelete 1

#endif 
