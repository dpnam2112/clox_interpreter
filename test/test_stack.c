#include "test.h"
#include "vm.h"

double VM_INIT() {
  vm_init();
  int st_size = vm_stack_size();
  vm_free();
  return st_size;
}

double ADD_TWO_ITEMS() {
  vm_init();
  vm_stack_push(1);
  vm_stack_push(2);
  int st_size = vm_stack_size();
  vm_free();
  return st_size;
}

double ADD_FOUR_ITEMS() {
  vm_init();
  for (int i = 0; i < 4; i++)
    vm_stack_push(i);
  int st_size = vm_stack_size();
  vm_free();
  return st_size;
}
double ADD_14_ITEMS_AND_POP_9() {
  vm_init();
  for (int i = 0; i < 14; i++)
    vm_stack_push(i);
  for (int i = 0; i < 9; i++)
    vm_stack_pop();

  int st_size = vm_stack_size();
  vm_free();
  return st_size;
}

Value TOP_ITEM_IS_19() {
  vm_init();
  for (int i = 0; i < 23; i++)
    vm_stack_push(i);

  vm_stack_pop();
  vm_stack_pop();
  vm_stack_pop();

  Value top = vm_stack_pop();
  vm_free();
  return top;
}

Value ADD_19_ITEMS_AND_POP_ALL() {
  vm_init();
  for (int i = 0; i < 19; i++)
    vm_stack_push(i);

  for (int i = 0; i < 19; i++)
    vm_stack_pop();

  Value result = vm_stack_size();
  vm_free();
  return result;
}

Value ADD_20_ITEMS_POP_3_ITEMS_ADD_17_ITEMS_POP_4_ITEMS() {
  vm_init();
  for (int i = 0; i < 20; i++)
    vm_stack_push(i);

  for (int i = 0; i < 3; i++)
    vm_stack_pop();

  for (int i = 0; i < 17; i++)
    vm_stack_push(i);

  for (int i = 0; i < 4; i++)
    vm_stack_pop();

  Value result = vm_stack_size();
  vm_free();
  return result;
}

Value TOP_ITEM_IS_2() {
  vm_init();
  for (int i = 0; i < 10; i++)
    vm_stack_push(i);

  for (int i = 0; i < 5; i++)
    vm_stack_pop();

  for (int i = 0; i < 6; i++)
    vm_stack_push(i);

  for (int i = 0; i < 3; i++)
    vm_stack_pop();

  Value top = vm_stack_pop();
  vm_free();
  return top;
}

void test_stack() {
  printf("== begin testing stack operations ==\n");

  test(VM_INIT, "vm init", 0);
  test(ADD_TWO_ITEMS, "add two items", 2);
  test(ADD_FOUR_ITEMS, "add four items", 4);
  test(ADD_14_ITEMS_AND_POP_9, "add 14 items and pop 9 items", 5);
  test(TOP_ITEM_IS_19, "Top item is 19", 19);
  test(ADD_19_ITEMS_AND_POP_ALL, "Add 19 items, then pop all items", 0);
  test(ADD_20_ITEMS_POP_3_ITEMS_ADD_17_ITEMS_POP_4_ITEMS,
       "Add 20 items, pop 3 items, add 17 items, pop 4 items", 30);
  test(TOP_ITEM_IS_2, "Expect top item equal to 2", 2);
  printf("== end of test ==\n");
}
