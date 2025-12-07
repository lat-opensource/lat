#ifndef _JRRA_TRANSLATOR_H_
#define _JRRA_TRANSLATOR_H_

#include "jrra.h"
#include "translate.h"

void jrra_emit_call_metadata(TranslationBlock *tb, target_ulong next_pc,
                             JrraCallType call_type);
bool jrra_translate_ret(TranslationBlock *tb, IR2_OPND return_addr_opnd,
                        IR1_INST *pir1);
#endif
