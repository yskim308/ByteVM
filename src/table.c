#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void init_table(Table *table) {
  table->count = 0;
  table->capcity = 0;
  table->entries = NULL;
}

void free_table(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capcity);
  init_table(table);
}
