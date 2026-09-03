.386
.model flat
option casemap:none

EXTERN _W2K_DecodePointer@4:PROC
EXTERN _W2K_EncodePointer@4:PROC
EXTERN _W2K_GetModuleHandleExA@12:PROC
EXTERN _W2K_GetModuleHandleExW@12:PROC
EXTERN _W2K_InitializeSListHead@4:PROC

_DATA SEGMENT
PUBLIC __imp__DecodePointer@4
PUBLIC __imp__EncodePointer@4
PUBLIC __imp__GetModuleHandleExA@12
PUBLIC __imp__GetModuleHandleExW@12
PUBLIC __imp__InitializeSListHead@4

__imp__DecodePointer@4 DD OFFSET _W2K_DecodePointer@4
__imp__EncodePointer@4 DD OFFSET _W2K_EncodePointer@4
__imp__GetModuleHandleExA@12 DD OFFSET _W2K_GetModuleHandleExA@12
__imp__GetModuleHandleExW@12 DD OFFSET _W2K_GetModuleHandleExW@12
__imp__InitializeSListHead@4 DD OFFSET _W2K_InitializeSListHead@4
_DATA ENDS

END
