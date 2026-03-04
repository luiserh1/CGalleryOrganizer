#include <stdlib.h>

#include "systems/renamer_history.h"

void RenamerHistoryFreeEntries(RenamerHistoryEntry *entries) { free(entries); }
