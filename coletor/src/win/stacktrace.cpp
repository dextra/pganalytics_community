#include <cstdio>
#include <windows.h>
#include <imagehlp.h>

// Source: http://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
// To get addr2line: for p in pos1 pos2 ...; do i586-mingw32msvc-addr2line -f -e test.exe $p; done
// To compile: i586-mingw32msvc-g++ -g3 test.cpp -limagehlp -o test.exe

LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS * ExceptionInfo);
void windows_print_stacktrace(CONTEXT* context);

LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS * ExceptionInfo)
{
	switch(ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		case EXCEPTION_ACCESS_VIOLATION:
			fputs("Error: EXCEPTION_ACCESS_VIOLATION\n", stderr);
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			fputs("Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n", stderr);
			break;
		case EXCEPTION_BREAKPOINT:
			fputs("Error: EXCEPTION_BREAKPOINT\n", stderr);
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			fputs("Error: EXCEPTION_DATATYPE_MISALIGNMENT\n", stderr);
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			fputs("Error: EXCEPTION_FLT_DENORMAL_OPERAND\n", stderr);
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			fputs("Error: EXCEPTION_FLT_DIVIDE_BY_ZERO\n", stderr);
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			fputs("Error: EXCEPTION_FLT_INEXACT_RESULT\n", stderr);
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			fputs("Error: EXCEPTION_FLT_INVALID_OPERATION\n", stderr);
			break;
		case EXCEPTION_FLT_OVERFLOW:
			fputs("Error: EXCEPTION_FLT_OVERFLOW\n", stderr);
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			fputs("Error: EXCEPTION_FLT_STACK_CHECK\n", stderr);
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			fputs("Error: EXCEPTION_FLT_UNDERFLOW\n", stderr);
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			fputs("Error: EXCEPTION_ILLEGAL_INSTRUCTION\n", stderr);
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			fputs("Error: EXCEPTION_IN_PAGE_ERROR\n", stderr);
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			fputs("Error: EXCEPTION_INT_DIVIDE_BY_ZERO\n", stderr);
			break;
		case EXCEPTION_INT_OVERFLOW:
			fputs("Error: EXCEPTION_INT_OVERFLOW\n", stderr);
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			fputs("Error: EXCEPTION_INVALID_DISPOSITION\n", stderr);
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			fputs("Error: EXCEPTION_NONCONTINUABLE_EXCEPTION\n", stderr);
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			fputs("Error: EXCEPTION_PRIV_INSTRUCTION\n", stderr);
			break;
		case EXCEPTION_SINGLE_STEP:
			fputs("Error: EXCEPTION_SINGLE_STEP\n", stderr);
			break;
		case EXCEPTION_STACK_OVERFLOW:
			fputs("Error: EXCEPTION_STACK_OVERFLOW\n", stderr);
			break;
		default:
			fputs("Error: Unrecognized Exception\n", stderr);
			break;
	}
	fflush(stderr);
	/* If this is a stack overflow then we can't walk the stack, so just show
	   where the error happened */
	if (EXCEPTION_STACK_OVERFLOW != ExceptionInfo->ExceptionRecord->ExceptionCode)
	{
		windows_print_stacktrace(ExceptionInfo->ContextRecord);
	}
	else
	{
		fprintf(stderr, "Stack Overflow at position: %p\n", (void*)ExceptionInfo->ContextRecord->Eip);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void setup_unhandled_exception_catch()
{
	SetUnhandledExceptionFilter(windows_exception_handler);
}

/*
void print_name(DWORD64 dwAddress)
{
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;
	if (!SymFromAddr(GetCurrentProcess(), dwAddress, &dwDisplacement, pSymbol))
	{
		printf("SymFromAddr returned error for : %p\n", (void*)dwAddress);
		return;
	}
	fprintf(stderr, "Symbol: %s (%p)\n", pSymbol->Name, dwAddress);
}
*/
void windows_print_stacktrace(CONTEXT* context)
{
	CONTEXT tmp_context;
	fprintf(stderr, "Stack:\n");
	if (context == NULL)
	{
		// Source: http://jpassing.com/2008/03/12/walking-the-stack-of-the-current-thread/
		ZeroMemory( &tmp_context, sizeof( CONTEXT ) );
		tmp_context.ContextFlags = CONTEXT_CONTROL;
		asm(
			"   call x;"
			"x: pop %%eax;"
			"   movl %%eax, %0;"
			"   movl %%ebp, %1;"
			"   movl %%esp, %2;"
			: "=r" (tmp_context.Eip), "=r" (tmp_context.Ebp), "=r" (tmp_context.Esp)
		);

		context = &tmp_context;
	}
	SymInitialize(GetCurrentProcess(), 0, true);

	STACKFRAME frame;
	memset(&frame, 0, sizeof(frame));

	/* setup initial stack frame */
	frame.AddrPC.Offset         = context->Eip;
	frame.AddrPC.Mode           = AddrModeFlat;
	frame.AddrStack.Offset      = context->Esp;
	frame.AddrStack.Mode        = AddrModeFlat;
	frame.AddrFrame.Offset      = context->Ebp;
	frame.AddrFrame.Mode        = AddrModeFlat;

	while (StackWalk(IMAGE_FILE_MACHINE_I386 ,
				GetCurrentProcess(),
				GetCurrentThread(),
				&frame,
				context,
				0,
				SymFunctionTableAccess,
				SymGetModuleBase,
				0 ) )
	{
		//addr2line(icky_global_program_name, (void*)frame.AddrPC.Offset);
		fprintf(stderr, "     %p\n", (void*)frame.AddrPC.Offset);
	}

	SymCleanup( GetCurrentProcess() );
}

