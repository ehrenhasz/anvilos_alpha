
 

#include <linux/export.h>
#include <linux/module.h>
#include "dfltcc_util.h"
#include "dfltcc.h"

char *oesc_msg(
    char *buf,
    int oesc
)
{
    if (oesc == 0x00)
        return NULL;  
    else {
#ifdef STATIC
        return NULL;  
#else
        sprintf(buf, "Operation-Ending-Supplemental Code is 0x%.2X", oesc);
        return buf;
#endif
    }
}

void dfltcc_reset_state(struct dfltcc_state *dfltcc_state) {
     
    if (is_dfltcc_enabled()) {
        dfltcc(DFLTCC_QAF, &dfltcc_state->param, NULL, NULL, NULL, NULL, NULL);
        memmove(&dfltcc_state->af, &dfltcc_state->param, sizeof(dfltcc_state->af));
    } else
        memset(&dfltcc_state->af, 0, sizeof(dfltcc_state->af));

     
    memset(&dfltcc_state->param, 0, sizeof(dfltcc_state->param));
    dfltcc_state->param.nt = 1;
    dfltcc_state->param.ribm = DFLTCC_RIBM;
}

MODULE_LICENSE("GPL");
