//This file is autogenerated for
//    SystemMessageHandler.java
//put this file at the end of the include list
//so the type definition used in this file will be found
#ifndef ELASTOS_SYSTEMMESSAGEHANDLER_CALLBACK_DEC_HH
#define ELASTOS_SYSTEMMESSAGEHANDLER_CALLBACK_DEC_HH


#ifdef __cplusplus
extern "C"
{
#endif
    extern void Elastos_SystemMessageHandler_nativeDoRunLoopOnce(IInterface* caller,Int64 messagePumpDelegateNative,Int64 delayedScheduledTimeTicks);
#ifdef __cplusplus
}
#endif


struct ElaSystemMessageHandlerCallback
{
    void (*elastos_SystemMessageHandler_scheduleWork)(IInterface* obj);
    void (*elastos_SystemMessageHandler_scheduleDelayedWork)(IInterface* obj, Int64 delayedTimeTicks, Int64 millis);
    void (*elastos_SystemMessageHandler_removeAllPendingMessages)(IInterface* obj);
    AutoPtr<IInterface> (*elastos_SystemMessageHandler_create)(Int64 messagePumpDelegateNative);
};


#endif //ELASTOS_SYSTEMMESSAGEHANDLER_CALLBACK_DEC_HH
