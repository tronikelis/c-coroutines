#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Linux x86_64 call convention
// %rdi, %rsi, %rdx, %rcx, %r8, and %r9

typedef struct Coroutine {
    void** stack;
    void** rsp;
    bool dead;
} Coroutine;

typedef struct CoroutineStack {
    Coroutine* cs[1024];
    int ptr;
} CoroutineStack;

CoroutineStack* coroutine_stack;

void __attribute__((naked)) coroutine_yield(void* arg) {
    asm("pushq %rbp\n"
        "movq %rsp, %rsi\n"
        "jmp coroutine_restore_context\n");
}

void coroutine_exit() {
    Coroutine* c = coroutine_stack->cs[coroutine_stack->ptr];
    c->dead = true;
    coroutine_yield(NULL);
}

Coroutine* coroutine_new(void (*fn)()) {
    Coroutine* c = malloc(sizeof(Coroutine));

    c->dead = false;
    c->stack = malloc(4096); // stack grows downwards
    void** rsp = c->stack + 4096;

    *(--rsp) = coroutine_exit;
    // top of stack return address
    *(--rsp) = fn;
    // rbp
    *(--rsp) = NULL; // on first return to `fn` rbp will be 0, but that's fine
                     // because calling convention sets rsp to rbp before
                     // proceeding with the function

    c->rsp = rsp;

    return c;
}

void __attribute__((naked)) coroutine_switch_context_finalize(void* rsp) {
    asm("movq %rdi, %rsp\n"
        "popq %rbp\n"
        "retq\n");
}

void coroutine_switch_context(Coroutine* c, void* caller_rsp) {
    coroutine_stack->cs[coroutine_stack->ptr]->rsp = caller_rsp;

    coroutine_stack->ptr++;
    coroutine_stack->cs[coroutine_stack->ptr] = c;

    coroutine_switch_context_finalize(c->rsp);
}

void __attribute__((naked)) coroutine_restore_context_finalize(void* arg,
                                                               void* rsp) {
    asm("movq %rdi, %rax\n"
        "movq %rsi, %rsp\n"
        "popq %rbp\n"
        "retq\n");
}

void coroutine_restore_context(void* arg, void* rsp) {
    coroutine_stack->cs[coroutine_stack->ptr]->rsp = rsp;

    coroutine_stack->ptr--;

    coroutine_restore_context_finalize(
        arg, coroutine_stack->cs[coroutine_stack->ptr]->rsp);
}

void* __attribute__((naked)) coroutine_next(Coroutine* c) {
    asm("pushq %rbp\n"
        "movq %rsp, %rsi\n"
        "jmp coroutine_switch_context\n"); // coroutine_switch_context(c, rsp)
}

void forever() {
    long i = 69;
    while (i < 100) {
        coroutine_yield((void*)i++);
    }
}

int main() {
    coroutine_stack = malloc(sizeof(CoroutineStack));
    coroutine_stack->cs[0] = coroutine_new(NULL);
    coroutine_stack->ptr = 0;

    printf("main\n");
    Coroutine* c = coroutine_new(forever);

    while (true) {
        void* answer = coroutine_next(c);

        if (c->dead) {
            break;
        }

        long i = (long)answer;
        printf("NAAAAH: %ld\n", i);
    }

    // freeing memory is kernel job :))
    return 0;
}
