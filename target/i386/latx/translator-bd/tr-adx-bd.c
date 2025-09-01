#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"

bool translate_adcx_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND dest_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND flag_opnd = ra_alloc_itemp();
    IR2_OPND cflag_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();

    la_x86mfflag(flag_opnd, 0x3f);
    la_bstrpick_d(cflag_opnd, flag_opnd, 0, 0);

    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) {
        la_add_w(temp1_opnd, dest_opnd, src_opnd);
        la_add_w(temp1_opnd, temp1_opnd, cflag_opnd);
        la_x86adc_w(dest_opnd, src_opnd);
    } else {
        la_add_d(temp1_opnd, dest_opnd, src_opnd);
        la_add_d(temp1_opnd, temp1_opnd, cflag_opnd);
        la_x86adc_d(dest_opnd, src_opnd);
    }
    la_x86mtflag(flag_opnd, 0x3e); //cf is not recovered

    store_ireg_to_ir1_bd(temp1_opnd, ir1_get_opnd_bd(pir1, 0), false);
    return true;
}

bool translate_adox_bd(IR1_INST *pir1)
{
    IR2_OPND src_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND dest_opnd = load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0), ZERO_EXTENSION, false);
    IR2_OPND flag_opnd = ra_alloc_itemp();
    IR2_OPND oflag_opnd = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();

    /* save eflags and set of -> cf */
    la_x86mfflag(flag_opnd, 0x3f);
    la_bstrpick_d(oflag_opnd, flag_opnd, 11, 11);
    la_x86mtflag(oflag_opnd, 0x1);

    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) {
        la_add_w(temp1_opnd, dest_opnd, src_opnd);
        la_add_w(temp1_opnd, temp1_opnd, oflag_opnd);
        la_x86adc_w(dest_opnd, src_opnd);
    } else {
        la_add_d(temp1_opnd, dest_opnd, src_opnd);
        la_add_d(temp1_opnd, temp1_opnd, oflag_opnd);
        la_x86adc_d(dest_opnd, src_opnd);
    }
    /* set cf -> of */
    la_x86mfflag(oflag_opnd, 0x1);
    la_bstrins_d(flag_opnd, oflag_opnd, 11, 11);
    la_x86mtflag(flag_opnd, 0x3f);

    store_ireg_to_ir1_bd(temp1_opnd, ir1_get_opnd_bd(pir1, 0), false);
    return true;
}
