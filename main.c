#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define STACK_MAX 256
#define INITIAL_GC_THRESHOLD 8

typedef enum {
    OBJ_INT,
    OBJ_PAIR,
} object_type;

typedef struct object {
    object_type Type;

    union {
        int Int;

        struct {
            struct object* Head;
            struct object* Tail;
        } Pair;
    } As;

    // For GC
    bool            Marked;
    struct object*  Next;
} object;

typedef struct {
    object* Stack[STACK_MAX];
    int     StackSize;

    // For GC
    object* FirstObject;
    int     NumObjects;
    int     MaxObjects;
} vm;

vm* vm_new() {
    vm* VM = (vm*)malloc(sizeof(vm));

    *VM = (vm) {
        .StackSize    = 0,
        .FirstObject  = NULL,
        .NumObjects   = 0,
        .MaxObjects   = INITIAL_GC_THRESHOLD,
    };

    return VM;
}

void vm_gc(vm* VM);

object* vm_new_object(vm* VM, object_type Type) {
    if (VM->NumObjects == VM->MaxObjects) vm_gc(VM);

    object* Object = (object*)malloc(sizeof(object));

    *Object = (object) {
        .Type   = Type,
        .Marked = false,
        .Next   = VM->FirstObject,
    };

    VM->FirstObject = Object;
    VM->NumObjects++;

    return Object;
}

void vm_stack_push(vm* VM, object* Value) {
    assert(VM->StackSize < STACK_MAX && "Stack overflow");

    VM->Stack[VM->StackSize++] = Value;
}

object* vm_pop(vm* VM) {
    assert(VM->StackSize > 0 && "Stack underflow");

    return VM->Stack[--VM->StackSize];
}

void vm_push_int(vm* VM, int Value) {
    object* Object = vm_new_object(VM, OBJ_INT);
    Object->As.Int = Value;

    vm_stack_push(VM, Object);
}

object* vm_push_pair(vm* VM) {
    object* Object = vm_new_object(VM, OBJ_PAIR);
    Object->As.Pair.Tail = vm_pop(VM);
    Object->As.Pair.Head = vm_pop(VM);

    vm_stack_push(VM, Object);

    return Object;
}

void vm_mark(object* Object) {
    // Avoid infinite loops
    if (Object->Marked) return;

    Object->Marked = true;

    if (Object->Type == OBJ_PAIR) {
        vm_mark(Object->As.Pair.Head);
        vm_mark(Object->As.Pair.Tail);
    }
}

void vm_mark_all(vm* VM) {
    for (int i = 0; i < VM->StackSize; i++) vm_mark(VM->Stack[i]);
}

void vm_sweep(vm* VM) {
    object** Object = &VM->FirstObject;

    while (*Object) {
        if (!(*Object)->Marked) {
            object* Unreached = *Object;

            *Object = Unreached->Next;
            free(Unreached);

            VM->NumObjects--;
        } else {
            (*Object)->Marked = false;
            Object = &(*Object)->Next;
        }
    }
}

void vm_gc(vm* VM) {
    int NumObjects = VM->NumObjects;

    vm_mark_all(VM);
    vm_sweep(VM);

    VM->MaxObjects = VM->NumObjects == 0 ? INITIAL_GC_THRESHOLD : VM->NumObjects * 2;

    printf("[DEBU] Collected %d objects, %d remaining.\n", NumObjects - VM->NumObjects, VM->NumObjects);
}

void vm_object_dump(object* Object) {
    switch (Object->Type) {
        case OBJ_INT:
            printf("%d", Object->As.Int);
            break;

        case OBJ_PAIR:
            printf("(");
            vm_object_dump(Object->As.Pair.Head);
            printf(", ");
            vm_object_dump(Object->As.Pair.Tail);
            printf(")");
            break;
    }
}

void vm_stack_dump(vm* VM) {
    printf("[DEBU] Stack:\n");

    for (int i = 0; i < VM->StackSize; i++) {
        printf("[DEBU]   %d: ", i);
        vm_object_dump(VM->Stack[i]);
        printf("\n");
    }
}

void vm_free(vm* VM) {
    VM->StackSize = 0;

    vm_gc(VM);
    free(VM);
}

void test1() {
    printf("[INFO] Test 1: Objects on stack are preserved.\n");

    vm* VM = vm_new();
    vm_push_int(VM, 1);
    vm_push_int(VM, 2);

    vm_stack_dump(VM);

    vm_gc(VM);
    assert(VM->NumObjects == 2);

    printf("[INFO] Test 1: Completed.\n");
    vm_free(VM);
}

void test2() {
    printf("[INFO] Test 2: Unreached objects are collected.\n");

    vm* VM = vm_new();
    vm_push_int(VM, 1);
    vm_push_int(VM, 2);
    vm_pop(VM);
    vm_pop(VM);

    vm_stack_dump(VM);

    vm_gc(VM);
    assert(VM->NumObjects == 0);

    printf("[INFO] Test 2: Completed.\n");
    vm_free(VM);
}

void test3() {
    printf("[INFO] Test 3: Reach nested objects.\n");

    vm* VM = vm_new();
    vm_push_int(VM, 1);
    vm_push_int(VM, 2);
    vm_push_pair(VM);
    vm_push_int(VM, 3);
    vm_push_int(VM, 4);
    vm_push_pair(VM);
    vm_push_pair(VM);

    vm_stack_dump(VM);

    vm_gc(VM);
    assert(VM->NumObjects == 7);

    printf("[INFO] Test 3: Completed.\n");
    vm_free(VM);
}

void test4() {
    printf("[INFO] Test 4: Handle cycles.\n");

    vm* VM = vm_new();
    vm_push_int(VM, 1);
    vm_push_int(VM, 2);
    object* A = vm_push_pair(VM);
    vm_push_int(VM, 3);
    vm_push_int(VM, 4);
    object* B = vm_push_pair(VM);

    // NOTE: we can't due this after setting up the cycle!
    vm_stack_dump(VM);

    // Set up a cycle, A -> B -> A
    A->As.Pair.Tail = B;
    B->As.Pair.Tail = A;

    vm_gc(VM);
    assert(VM->NumObjects == 4);

    printf("[INFO] Test 4: Completed.\n");
    vm_free(VM);
}

void test5() {
    printf("[INFO] Test 5: Collect cycles.\n");

    vm* VM = vm_new();
    vm_push_int(VM, 1);
    vm_push_int(VM, 2);
    object* A = vm_push_pair(VM);
    vm_push_int(VM, 3);
    vm_push_int(VM, 4);
    object* B = vm_push_pair(VM);

    // NOTE: we can't due this after setting up the cycle!
    vm_stack_dump(VM);

    // Set up a cycle, A -> B -> A
    A->As.Pair.Tail = B;
    B->As.Pair.Tail = A;

    vm_pop(VM);
    vm_pop(VM);

    vm_gc(VM);
    assert(VM->NumObjects == 0);

    printf("[INFO] Test 5: Completed.\n");
    vm_free(VM);
}

void test6() {
    printf("[INFO] Test 6: Stress test.\n");

    vm* VM = vm_new();

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 20; j++) {
            vm_push_int(VM, i);
        }

        for (int k = 0; k < 20; k++) {
            vm_pop(VM);
        }
    }

    vm_gc(VM);
    assert(VM->NumObjects == 0);

    printf("[INFO] Test 6: Completed.\n");
    vm_free(VM);
}

int main(void) {
    test1();
    test2();
    test3();
    test4();
    test5();
    // test6();

    return 0;
}
