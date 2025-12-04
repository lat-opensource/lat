#include "ir2.h"

#define LISA_FADD_H LISA_INVALID
#define LISA_FSUB_H LISA_INVALID
#define LISA_FMUL_H LISA_INVALID
#define LISA_FDIV_H LISA_INVALID
#define LISA_FMAX_H LISA_INVALID
#define LISA_FMIN_H LISA_INVALID
#define LISA_FMAXA_H LISA_INVALID
#define LISA_FMINA_H LISA_INVALID
#define LISA_FSCALEB_H LISA_INVALID
#define LISA_FCOPYSIGN_H LISA_INVALID
#define LISA_FABS_H LISA_INVALID
#define LISA_FNEG_H LISA_INVALID
#define LISA_FLOGB_H LISA_INVALID
#define LISA_FCLASS_H LISA_INVALID
#define LISA_FSQRT_H LISA_INVALID
#define LISA_FRECIP_H LISA_INVALID
#define LISA_FRSQRT_H LISA_INVALID
#define LISA_FRECIPE_H LISA_INVALID
#define LISA_FRECIPE_S LISA_INVALID
#define LISA_FRECIPE_D LISA_INVALID
#define LISA_FRSQRTE_H LISA_INVALID
#define LISA_FRSQRTE_S LISA_INVALID
#define LISA_FRSQRTE_D LISA_INVALID
#define LISA_FMOV_H LISA_INVALID
#define LISA_MOVGR2FR_H LISA_INVALID
#define LISA_MOVFR2GR_H LISA_INVALID
#define LISA_FCVTRM_H_S LISA_INVALID
#define LISA_FCVTRM_H_D LISA_INVALID
#define LISA_FCVTRM_H_Q LISA_INVALID
#define LISA_FCVTRM_S_H LISA_INVALID
#define LISA_FCVTRM_S_D LISA_INVALID
#define LISA_FCVTRM_S_Q LISA_INVALID
#define LISA_FCVTRM_D_H LISA_INVALID
#define LISA_FCVTRM_D_S LISA_INVALID
#define LISA_FCVTRM_D_Q LISA_INVALID
#define LISA_FCVTRM_Q_H LISA_INVALID
#define LISA_FCVTRM_Q_S LISA_INVALID
#define LISA_FCVTRM_Q_D LISA_INVALID
#define LISA_FCVTRP_H_S LISA_INVALID
#define LISA_FCVTRP_H_D LISA_INVALID
#define LISA_FCVTRP_H_Q LISA_INVALID
#define LISA_FCVTRP_S_H LISA_INVALID
#define LISA_FCVTRP_S_D LISA_INVALID
#define LISA_FCVTRP_S_Q LISA_INVALID
#define LISA_FCVTRP_D_H LISA_INVALID
#define LISA_FCVTRP_D_S LISA_INVALID
#define LISA_FCVTRP_D_Q LISA_INVALID
#define LISA_FCVTRP_Q_H LISA_INVALID
#define LISA_FCVTRP_Q_S LISA_INVALID
#define LISA_FCVTRP_Q_D LISA_INVALID
#define LISA_FCVTRZ_H_S LISA_INVALID
#define LISA_FCVTRZ_H_D LISA_INVALID
#define LISA_FCVTRZ_H_Q LISA_INVALID
#define LISA_FCVTRZ_S_H LISA_INVALID
#define LISA_FCVTRZ_S_D LISA_INVALID
#define LISA_FCVTRZ_S_Q LISA_INVALID
#define LISA_FCVTRZ_D_H LISA_INVALID
#define LISA_FCVTRZ_D_S LISA_INVALID
#define LISA_FCVTRZ_D_Q LISA_INVALID
#define LISA_FCVTRZ_Q_H LISA_INVALID
#define LISA_FCVTRZ_Q_S LISA_INVALID
#define LISA_FCVTRZ_Q_D LISA_INVALID
#define LISA_FCVTRNE_H_S LISA_INVALID
#define LISA_FCVTRNE_H_D LISA_INVALID
#define LISA_FCVTRNE_H_Q LISA_INVALID
#define LISA_FCVTRNE_S_H LISA_INVALID
#define LISA_FCVTRNE_S_D LISA_INVALID
#define LISA_FCVTRNE_S_Q LISA_INVALID
#define LISA_FCVTRNE_D_H LISA_INVALID
#define LISA_FCVTRNE_D_S LISA_INVALID
#define LISA_FCVTRNE_D_Q LISA_INVALID
#define LISA_FCVTRNE_Q_H LISA_INVALID
#define LISA_FCVTRNE_Q_S LISA_INVALID
#define LISA_FCVTRNE_Q_D LISA_INVALID
#define LISA_FCVT_H_S LISA_INVALID
#define LISA_FCVT_H_D LISA_INVALID
#define LISA_FCVT_H_Q LISA_INVALID
#define LISA_FCVT_S_H LISA_INVALID
#define LISA_FCVT_S_Q LISA_INVALID
#define LISA_FCVT_D_H LISA_INVALID
#define LISA_FCVT_D_Q LISA_INVALID
#define LISA_FCVT_Q_H LISA_INVALID
#define LISA_FCVT_Q_S LISA_INVALID
#define LISA_FCVT_Q_D LISA_INVALID
#define LISA_FTINTRM_W_H LISA_INVALID
#define LISA_FTINTRM_W_Q LISA_INVALID
#define LISA_FTINTRM_WU_H LISA_INVALID
#define LISA_FTINTRM_WU_S LISA_INVALID
#define LISA_FTINTRM_WU_D LISA_INVALID
#define LISA_FTINTRM_WU_Q LISA_INVALID
#define LISA_FTINTRM_L_H LISA_INVALID
#define LISA_FTINTRM_L_Q LISA_INVALID
#define LISA_FTINTRM_LU_H LISA_INVALID
#define LISA_FTINTRM_LU_S LISA_INVALID
#define LISA_FTINTRM_LU_D LISA_INVALID
#define LISA_FTINTRM_LU_Q LISA_INVALID
#define LISA_FTINTRP_W_H LISA_INVALID
#define LISA_FTINTRP_W_Q LISA_INVALID
#define LISA_FTINTRP_WU_H LISA_INVALID
#define LISA_FTINTRP_WU_S LISA_INVALID
#define LISA_FTINTRP_WU_D LISA_INVALID
#define LISA_FTINTRP_WU_Q LISA_INVALID
#define LISA_FTINTRP_L_H LISA_INVALID
#define LISA_FTINTRP_L_Q LISA_INVALID
#define LISA_FTINTRP_LU_H LISA_INVALID
#define LISA_FTINTRP_LU_S LISA_INVALID
#define LISA_FTINTRP_LU_D LISA_INVALID
#define LISA_FTINTRP_LU_Q LISA_INVALID
#define LISA_FTINTRZ_W_H LISA_INVALID
#define LISA_FTINTRZ_W_Q LISA_INVALID
#define LISA_FTINTRZ_WU_H LISA_INVALID
#define LISA_FTINTRZ_WU_S LISA_INVALID
#define LISA_FTINTRZ_WU_D LISA_INVALID
#define LISA_FTINTRZ_WU_Q LISA_INVALID
#define LISA_FTINTRZ_L_H LISA_INVALID
#define LISA_FTINTRZ_L_Q LISA_INVALID
#define LISA_FTINTRZ_LU_H LISA_INVALID
#define LISA_FTINTRZ_LU_S LISA_INVALID
#define LISA_FTINTRZ_LU_D LISA_INVALID
#define LISA_FTINTRZ_LU_Q LISA_INVALID
#define LISA_FTINTRNE_W_H LISA_INVALID
#define LISA_FTINTRNE_W_Q LISA_INVALID
#define LISA_FTINTRNE_WU_H LISA_INVALID
#define LISA_FTINTRNE_WU_S LISA_INVALID
#define LISA_FTINTRNE_WU_D LISA_INVALID
#define LISA_FTINTRNE_WU_Q LISA_INVALID
#define LISA_FTINTRNE_L_H LISA_INVALID
#define LISA_FTINTRNE_L_Q LISA_INVALID
#define LISA_FTINTRNE_LU_H LISA_INVALID
#define LISA_FTINTRNE_LU_S LISA_INVALID
#define LISA_FTINTRNE_LU_D LISA_INVALID
#define LISA_FTINTRNE_LU_Q LISA_INVALID
#define LISA_FTINT_W_H LISA_INVALID
#define LISA_FTINT_W_Q LISA_INVALID
#define LISA_FTINT_WU_H LISA_INVALID
#define LISA_FTINT_WU_S LISA_INVALID
#define LISA_FTINT_WU_D LISA_INVALID
#define LISA_FTINT_WU_Q LISA_INVALID
#define LISA_FTINT_L_H LISA_INVALID
#define LISA_FTINT_L_Q LISA_INVALID
#define LISA_FTINT_LU_H LISA_INVALID
#define LISA_FTINT_LU_S LISA_INVALID
#define LISA_FTINT_LU_D LISA_INVALID
#define LISA_FTINT_LU_Q LISA_INVALID
#define LISA_FFINTRM_H_W LISA_INVALID
#define LISA_FFINTRM_H_WU LISA_INVALID
#define LISA_FFINTRM_H_L LISA_INVALID
#define LISA_FFINTRM_H_LU LISA_INVALID
#define LISA_FFINTRM_S_W LISA_INVALID
#define LISA_FFINTRM_S_WU LISA_INVALID
#define LISA_FFINTRM_S_L LISA_INVALID
#define LISA_FFINTRM_S_LU LISA_INVALID
#define LISA_FFINTRM_D_W LISA_INVALID
#define LISA_FFINTRM_D_WU LISA_INVALID
#define LISA_FFINTRM_D_L LISA_INVALID
#define LISA_FFINTRM_D_LU LISA_INVALID
#define LISA_FFINTRM_Q_W LISA_INVALID
#define LISA_FFINTRM_Q_WU LISA_INVALID
#define LISA_FFINTRM_Q_L LISA_INVALID
#define LISA_FFINTRM_Q_LU LISA_INVALID
#define LISA_FFINTRP_H_W LISA_INVALID
#define LISA_FFINTRP_H_WU LISA_INVALID
#define LISA_FFINTRP_H_L LISA_INVALID
#define LISA_FFINTRP_H_LU LISA_INVALID
#define LISA_FFINTRP_S_W LISA_INVALID
#define LISA_FFINTRP_S_WU LISA_INVALID
#define LISA_FFINTRP_S_L LISA_INVALID
#define LISA_FFINTRP_S_LU LISA_INVALID
#define LISA_FFINTRP_D_W LISA_INVALID
#define LISA_FFINTRP_D_WU LISA_INVALID
#define LISA_FFINTRP_D_L LISA_INVALID
#define LISA_FFINTRP_D_LU LISA_INVALID
#define LISA_FFINTRP_Q_W LISA_INVALID
#define LISA_FFINTRP_Q_WU LISA_INVALID
#define LISA_FFINTRP_Q_L LISA_INVALID
#define LISA_FFINTRP_Q_LU LISA_INVALID
#define LISA_FFINTRZ_H_W LISA_INVALID
#define LISA_FFINTRZ_H_WU LISA_INVALID
#define LISA_FFINTRZ_H_L LISA_INVALID
#define LISA_FFINTRZ_H_LU LISA_INVALID
#define LISA_FFINTRZ_S_W LISA_INVALID
#define LISA_FFINTRZ_S_WU LISA_INVALID
#define LISA_FFINTRZ_S_L LISA_INVALID
#define LISA_FFINTRZ_S_LU LISA_INVALID
#define LISA_FFINTRZ_D_W LISA_INVALID
#define LISA_FFINTRZ_D_WU LISA_INVALID
#define LISA_FFINTRZ_D_L LISA_INVALID
#define LISA_FFINTRZ_D_LU LISA_INVALID
#define LISA_FFINTRZ_Q_W LISA_INVALID
#define LISA_FFINTRZ_Q_WU LISA_INVALID
#define LISA_FFINTRZ_Q_L LISA_INVALID
#define LISA_FFINTRZ_Q_LU LISA_INVALID
#define LISA_FFINTRNE_H_W LISA_INVALID
#define LISA_FFINTRNE_H_WU LISA_INVALID
#define LISA_FFINTRNE_H_L LISA_INVALID
#define LISA_FFINTRNE_H_LU LISA_INVALID
#define LISA_FFINTRNE_S_W LISA_INVALID
#define LISA_FFINTRNE_S_WU LISA_INVALID
#define LISA_FFINTRNE_S_L LISA_INVALID
#define LISA_FFINTRNE_S_LU LISA_INVALID
#define LISA_FFINTRNE_D_W LISA_INVALID
#define LISA_FFINTRNE_D_WU LISA_INVALID
#define LISA_FFINTRNE_D_L LISA_INVALID
#define LISA_FFINTRNE_D_LU LISA_INVALID
#define LISA_FFINTRNE_Q_W LISA_INVALID
#define LISA_FFINTRNE_Q_WU LISA_INVALID
#define LISA_FFINTRNE_Q_L LISA_INVALID
#define LISA_FFINTRNE_Q_LU LISA_INVALID
#define LISA_FFINT_H_W LISA_INVALID
#define LISA_FFINT_H_WU LISA_INVALID
#define LISA_FFINT_H_L LISA_INVALID
#define LISA_FFINT_H_LU LISA_INVALID
#define LISA_FFINT_S_WU LISA_INVALID
#define LISA_FFINT_S_LU LISA_INVALID
#define LISA_FFINT_D_WU LISA_INVALID
#define LISA_FFINT_D_LU LISA_INVALID
#define LISA_FFINT_Q_W LISA_INVALID
#define LISA_FFINT_Q_WU LISA_INVALID
#define LISA_FFINT_Q_L LISA_INVALID
#define LISA_FFINT_Q_LU LISA_INVALID
#define LISA_FRINTRM_H LISA_INVALID
#define LISA_FRINTRM_S LISA_INVALID
#define LISA_FRINTRM_D LISA_INVALID
#define LISA_FRINTRM_Q LISA_INVALID
#define LISA_FRINTRP_H LISA_INVALID
#define LISA_FRINTRP_S LISA_INVALID
#define LISA_FRINTRP_D LISA_INVALID
#define LISA_FRINTRP_Q LISA_INVALID
#define LISA_FRINTRZ_H LISA_INVALID
#define LISA_FRINTRZ_S LISA_INVALID
#define LISA_FRINTRZ_D LISA_INVALID
#define LISA_FRINTRZ_Q LISA_INVALID
#define LISA_FRINTRNE_H LISA_INVALID
#define LISA_FRINTRNE_S LISA_INVALID
#define LISA_FRINTRNE_D LISA_INVALID
#define LISA_FRINTRNE_Q LISA_INVALID
#define LISA_FRINT_H LISA_INVALID
#define LISA_FRINT_Q LISA_INVALID
#define LISA_CSRRD LISA_INVALID
#define LISA_CSRWR LISA_INVALID
#define LISA_CSRXCHG LISA_INVALID
#define LISA_GCSRRD LISA_INVALID
#define LISA_GCSRWR LISA_INVALID
#define LISA_GCSRXCHG LISA_INVALID
#define LISA_CACHE LISA_INVALID
#define LISA_LDDIR LISA_INVALID
#define LISA_LDPTE LISA_INVALID
#define LISA_IOCSRRD_B LISA_INVALID
#define LISA_IOCSRRD_H LISA_INVALID
#define LISA_IOCSRRD_W LISA_INVALID
#define LISA_IOCSRRD_D LISA_INVALID
#define LISA_IOCSRWR_B LISA_INVALID
#define LISA_IOCSRWR_H LISA_INVALID
#define LISA_IOCSRWR_W LISA_INVALID
#define LISA_IOCSRWR_D LISA_INVALID
#define LISA_TLBINV LISA_INVALID
#define LISA_GTLBINV LISA_INVALID
#define LISA_TLBFLUSH LISA_INVALID
#define LISA_GTLBFLUSH LISA_INVALID
#define LISA_TLBP LISA_INVALID
#define LISA_GTLBP LISA_INVALID
#define LISA_TLBR LISA_INVALID
#define LISA_GTLBR LISA_INVALID
#define LISA_TLBWI LISA_INVALID
#define LISA_GTLBWI LISA_INVALID
#define LISA_TLBWR LISA_INVALID
#define LISA_GTLBWR LISA_INVALID
#define LISA_ERET LISA_INVALID
#define LISA_DERET LISA_INVALID
#define LISA_WAIT LISA_INVALID
#define LISA_FCMP_COND_H LISA_INVALID
#define LISA_FCMP_CAF_S LISA_INVALID
#define LISA_FCMP_SAF_S LISA_INVALID
#define LISA_FCMP_CLT_S LISA_INVALID
#define LISA_FCMP_SLT_S LISA_INVALID
#define LISA_FCMP_CEQ_S LISA_INVALID
#define LISA_FCMP_SEQ_S LISA_INVALID
#define LISA_FCMP_CLE_S LISA_INVALID
#define LISA_FCMP_SLE_S LISA_INVALID
#define LISA_FCMP_CUN_S LISA_INVALID
#define LISA_FCMP_SUN_S LISA_INVALID
#define LISA_FCMP_CULT_S LISA_INVALID
#define LISA_FCMP_SULT_S LISA_INVALID
#define LISA_FCMP_CUEQ_S LISA_INVALID
#define LISA_FCMP_SUEQ_S LISA_INVALID
#define LISA_FCMP_CULE_S LISA_INVALID
#define LISA_FCMP_SULE_S LISA_INVALID
#define LISA_FCMP_CNE_S LISA_INVALID
#define LISA_FCMP_SNE_S LISA_INVALID
#define LISA_FCMP_COR_S LISA_INVALID
#define LISA_FCMP_SOR_S LISA_INVALID
#define LISA_FCMP_CUNE_S LISA_INVALID
#define LISA_FCMP_SUNE_S LISA_INVALID
#define LISA_FCMP_CAF_D LISA_INVALID
#define LISA_FCMP_SAF_D LISA_INVALID
#define LISA_FCMP_CLT_D LISA_INVALID
#define LISA_FCMP_SLT_D LISA_INVALID
#define LISA_FCMP_CEQ_D LISA_INVALID
#define LISA_FCMP_SEQ_D LISA_INVALID
#define LISA_FCMP_CLE_D LISA_INVALID
#define LISA_FCMP_SLE_D LISA_INVALID
#define LISA_FCMP_CUN_D LISA_INVALID
#define LISA_FCMP_SUN_D LISA_INVALID
#define LISA_FCMP_CULT_D LISA_INVALID
#define LISA_FCMP_SULT_D LISA_INVALID
#define LISA_FCMP_CUEQ_D LISA_INVALID
#define LISA_FCMP_SUEQ_D LISA_INVALID
#define LISA_FCMP_CULE_D LISA_INVALID
#define LISA_FCMP_SULE_D LISA_INVALID
#define LISA_FCMP_CNE_D LISA_INVALID
#define LISA_FCMP_SNE_D LISA_INVALID
#define LISA_FCMP_COR_D LISA_INVALID
#define LISA_FCMP_SOR_D LISA_INVALID
#define LISA_FCMP_CUNE_D LISA_INVALID
#define LISA_FCMP_SUNE_D LISA_INVALID
#define LISA_VADDW_Q_Q_D LISA_INVALID
#define LISA_VADDW_Q_Q_DU LISA_INVALID
#define LISA_VSUBW_Q_Q_D LISA_INVALID
#define LISA_VSUBW_Q_Q_DU LISA_INVALID
#define LISA_VSADDW_Q_Q_D LISA_INVALID
#define LISA_VSADDW_QU_QU_DU LISA_INVALID
#define LISA_VSSUBW_Q_Q_D LISA_INVALID
#define LISA_VSSUBW_QU_QU_DU LISA_INVALID
#define LISA_VFADD_H LISA_INVALID
#define LISA_VFADD_Q LISA_INVALID
#define LISA_VFSUB_H LISA_INVALID
#define LISA_VFSUB_Q LISA_INVALID
#define LISA_VFADDSUB_H LISA_INVALID
#define LISA_VFADDSUB_Q LISA_INVALID
#define LISA_VFSUBADD_H LISA_INVALID
#define LISA_VFSUBADD_Q LISA_INVALID
#define LISA_VFMUL_H LISA_INVALID
#define LISA_VFMUL_Q LISA_INVALID
#define LISA_VFDIV_H LISA_INVALID
#define LISA_VFDIV_Q LISA_INVALID
#define LISA_VFMAX_H LISA_INVALID
#define LISA_VFMAX_Q LISA_INVALID
#define LISA_VFMIN_H LISA_INVALID
#define LISA_VFMIN_Q LISA_INVALID
#define LISA_VFMAXA_H LISA_INVALID
#define LISA_VFMAXA_Q LISA_INVALID
#define LISA_VFMINA_H LISA_INVALID
#define LISA_VFMINA_Q LISA_INVALID
#define LISA_VFSCALEB_H LISA_INVALID
#define LISA_VFSCALEB_Q LISA_INVALID
#define LISA_VFCVT_D_Q LISA_INVALID
#define LISA_VFFINT_H_W LISA_INVALID
#define LISA_VFFINT_H_WU LISA_INVALID
#define LISA_VFFINT_S_LU LISA_INVALID
#define LISA_VFTINT_WU_D LISA_INVALID
#define LISA_VFTINTRM_WU_D LISA_INVALID
#define LISA_VFTINTRP_WU_D LISA_INVALID
#define LISA_VFTINTRZ_WU_D LISA_INVALID
#define LISA_VFTINTRNE_WU_D LISA_INVALID
#define LISA_VMSKGEZ_H LISA_INVALID
#define LISA_VMSKGEZ_W LISA_INVALID
#define LISA_VMSKGEZ_D LISA_INVALID
#define LISA_VMSKNZ_H LISA_INVALID
#define LISA_VMSKNZ_W LISA_INVALID
#define LISA_VMSKNZ_D LISA_INVALID
#define LISA_VMSKCOPY_H LISA_INVALID
#define LISA_VMSKCOPY_W LISA_INVALID
#define LISA_VMSKCOPY_D LISA_INVALID
#define LISA_VMSKFILL_H LISA_INVALID
#define LISA_VMSKFILL_W LISA_INVALID
#define LISA_VMSKFILL_D LISA_INVALID
#define LISA_VFLOGB_H LISA_INVALID
#define LISA_VFLOGB_Q LISA_INVALID
#define LISA_VFCLASS_H LISA_INVALID
#define LISA_VFCLASS_Q LISA_INVALID
#define LISA_VFSQRT_H LISA_INVALID
#define LISA_VFSQRT_Q LISA_INVALID
#define LISA_VFRECIP_H LISA_INVALID
#define LISA_VFRECIP_Q LISA_INVALID
#define LISA_VFRSQRT_H LISA_INVALID
#define LISA_VFRSQRT_Q LISA_INVALID
#define LISA_VFRECIPE_H LISA_INVALID
#define LISA_VFRECIPE_S LISA_INVALID
#define LISA_VFRECIPE_D LISA_INVALID
#define LISA_VFRECIPE_Q LISA_INVALID
#define LISA_VFRSQRTE_H LISA_INVALID
#define LISA_VFRSQRTE_S LISA_INVALID
#define LISA_VFRSQRTE_D LISA_INVALID
#define LISA_VFRSQRTE_Q LISA_INVALID
#define LISA_VFRINT_H LISA_INVALID
#define LISA_VFRINT_Q LISA_INVALID
#define LISA_VFRINTRM_H LISA_INVALID
#define LISA_VFRINTRM_Q LISA_INVALID
#define LISA_VFRINTRP_H LISA_INVALID
#define LISA_VFRINTRP_Q LISA_INVALID
#define LISA_VFRINTRZ_H LISA_INVALID
#define LISA_VFRINTRZ_Q LISA_INVALID
#define LISA_VFRINTRNE_H LISA_INVALID
#define LISA_VFRINTRNE_Q LISA_INVALID
#define LISA_VEXTL_Q_B LISA_INVALID
#define LISA_VEXTL_Q_H LISA_INVALID
#define LISA_VEXTL_Q_W LISA_INVALID
#define LISA_VEXTL_Q_BU LISA_INVALID
#define LISA_VEXTL_Q_HU LISA_INVALID
#define LISA_VEXTL_Q_WU LISA_INVALID
#define LISA_VFCVTL_Q_D LISA_INVALID
#define LISA_VFCVTH_Q_D LISA_INVALID
#define LISA_VFFINTL_Q_L LISA_INVALID
#define LISA_VFFINTH_Q_L LISA_INVALID
#define LISA_VFFINTL_D_WU LISA_INVALID
#define LISA_VFFINTH_D_WU LISA_INVALID
#define LISA_VFFINTL_Q_LU LISA_INVALID
#define LISA_VFFINTH_Q_LU LISA_INVALID
#define LISA_VFTINTRM_WU_S LISA_INVALID
#define LISA_VFTINTRM_LU_D LISA_INVALID
#define LISA_VFTINTRP_WU_S LISA_INVALID
#define LISA_VFTINTRP_LU_D LISA_INVALID
#define LISA_VFTINTRNE_WU_S LISA_INVALID
#define LISA_VFTINTRNE_LU_D LISA_INVALID
#define LISA_VFTINTL_LU_S LISA_INVALID
#define LISA_VFTINTH_LU_S LISA_INVALID
#define LISA_VFTINTRML_LU_S LISA_INVALID
#define LISA_VFTINTRMH_LU_S LISA_INVALID
#define LISA_VFTINTRPL_LU_S LISA_INVALID
#define LISA_VFTINTRPH_LU_S LISA_INVALID
#define LISA_VFTINTRZL_LU_S LISA_INVALID
#define LISA_VFTINTRZH_LU_S LISA_INVALID
#define LISA_VFTINTRNEL_LU_S LISA_INVALID
#define LISA_VFTINTRNEH_LU_S LISA_INVALID
#define LISA_VSLLWIL_Q_D LISA_INVALID
#define LISA_VSLLWIL_QU_DU LISA_INVALID
#define LISA_XVADDW_Q_Q_D LISA_INVALID
#define LISA_XVADDW_Q_Q_DU LISA_INVALID
#define LISA_XVSUBW_Q_Q_D LISA_INVALID
#define LISA_XVSUBW_Q_Q_DU LISA_INVALID
#define LISA_XVSADDW_Q_Q_D LISA_INVALID
#define LISA_XVSADDW_QU_QU_DU LISA_INVALID
#define LISA_XVSSUBW_Q_Q_D LISA_INVALID
#define LISA_XVSSUBW_QU_QU_DU LISA_INVALID

#define LISA_XVFADD_H LISA_INVALID
#define LISA_XVFADD_Q LISA_INVALID
#define LISA_XVFSUB_H LISA_INVALID
#define LISA_XVFSUB_Q LISA_INVALID
#define LISA_XVFADDSUB_H LISA_INVALID
#define LISA_XVFADDSUB_Q LISA_INVALID
#define LISA_XVFSUBADD_H LISA_INVALID
#define LISA_XVFSUBADD_Q LISA_INVALID
#define LISA_XVFMUL_H LISA_INVALID
#define LISA_XVFMUL_Q LISA_INVALID
#define LISA_XVFDIV_H LISA_INVALID
#define LISA_XVFDIV_Q LISA_INVALID
#define LISA_XVFMAX_H LISA_INVALID
#define LISA_XVFMAX_Q LISA_INVALID
#define LISA_XVFMIN_H LISA_INVALID
#define LISA_XVFMIN_Q LISA_INVALID
#define LISA_XVFMAXA_H LISA_INVALID
#define LISA_XVFMAXA_Q LISA_INVALID
#define LISA_XVFMINA_H LISA_INVALID
#define LISA_XVFMINA_Q LISA_INVALID
#define LISA_XVFSCALEB_H LISA_INVALID
#define LISA_XVFSCALEB_Q LISA_INVALID
#define LISA_XVFCVT_D_Q LISA_INVALID
#define LISA_XVFFINT_H_W LISA_INVALID
#define LISA_XVFFINT_H_WU LISA_INVALID
#define LISA_XVFFINT_S_LU LISA_INVALID
#define LISA_XVFTINT_WU_D LISA_INVALID
#define LISA_XVFTINTRM_WU_D LISA_INVALID
#define LISA_XVFTINTRP_WU_D LISA_INVALID
#define LISA_XVFTINTRZ_WU_D LISA_INVALID
#define LISA_XVFTINTRNE_WU_D LISA_INVALID

#define LISA_XVMSKGEZ_H LISA_INVALID
#define LISA_XVMSKGEZ_W LISA_INVALID
#define LISA_XVMSKGEZ_D LISA_INVALID
#define LISA_XVMSKNZ_H LISA_INVALID
#define LISA_XVMSKNZ_W LISA_INVALID
#define LISA_XVMSKNZ_D LISA_INVALID
#define LISA_XVMSKCOPY_H LISA_INVALID
#define LISA_XVMSKCOPY_W LISA_INVALID
#define LISA_XVMSKCOPY_D LISA_INVALID
#define LISA_XVMSKFILL_H LISA_INVALID
#define LISA_XVMSKFILL_W LISA_INVALID
#define LISA_XVMSKFILL_D LISA_INVALID

#define LISA_XVFLOGB_H LISA_INVALID
#define LISA_XVFLOGB_Q LISA_INVALID
#define LISA_XVFCLASS_H LISA_INVALID
#define LISA_XVFCLASS_Q LISA_INVALID
#define LISA_XVFSQRT_H LISA_INVALID
#define LISA_XVFSQRT_Q LISA_INVALID
#define LISA_XVFRECIP_H LISA_INVALID
#define LISA_XVFRECIP_Q LISA_INVALID
#define LISA_XVFRSQRT_H LISA_INVALID
#define LISA_XVFRSQRT_Q LISA_INVALID
#define LISA_XVFRECIPE_H LISA_INVALID
#define LISA_XVFRECIPE_S LISA_INVALID
#define LISA_XVFRECIPE_D LISA_INVALID
#define LISA_XVFRECIPE_Q LISA_INVALID
#define LISA_XVFRSQRTE_H LISA_INVALID
#define LISA_XVFRSQRTE_S LISA_INVALID
#define LISA_XVFRSQRTE_D LISA_INVALID
#define LISA_XVFRSQRTE_Q LISA_INVALID
#define LISA_XVFRINT_H LISA_INVALID
#define LISA_XVFRINT_Q LISA_INVALID
#define LISA_XVFRINTRM_H LISA_INVALID
#define LISA_XVFRINTRM_Q LISA_INVALID
#define LISA_XVFRINTRP_H LISA_INVALID
#define LISA_XVFRINTRP_Q LISA_INVALID
#define LISA_XVFRINTRZ_H LISA_INVALID
#define LISA_XVFRINTRZ_Q LISA_INVALID
#define LISA_XVFRINTRNE_H LISA_INVALID
#define LISA_XVFRINTRNE_Q LISA_INVALID

#define LISA_XVEXTL_Q_B LISA_INVALID
#define LISA_XVEXTL_Q_H LISA_INVALID
#define LISA_XVEXTL_Q_W LISA_INVALID
#define LISA_XVEXTL_Q_BU LISA_INVALID
#define LISA_XVEXTL_Q_HU LISA_INVALID
#define LISA_XVEXTL_Q_WU LISA_INVALID
#define LISA_XVFCVTL_Q_D LISA_INVALID
#define LISA_XVFCVTH_Q_D LISA_INVALID
#define LISA_XVFFINTL_Q_L LISA_INVALID
#define LISA_XVFFINTH_Q_L LISA_INVALID
#define LISA_XVFFINTL_D_WU LISA_INVALID
#define LISA_XVFFINTH_D_WU LISA_INVALID
#define LISA_XVFFINTL_Q_LU LISA_INVALID
#define LISA_XVFFINTH_Q_LU LISA_INVALID

#define LISA_XVFTINTRM_WU_S LISA_INVALID
#define LISA_XVFTINTRM_LU_D LISA_INVALID
#define LISA_XVFTINTRP_WU_S LISA_INVALID
#define LISA_XVFTINTRP_LU_D LISA_INVALID
#define LISA_XVFTINTRNE_WU_S LISA_INVALID
#define LISA_XVFTINTRNE_LU_D LISA_INVALID
#define LISA_XVFTINTL_LU_S LISA_INVALID
#define LISA_XVFTINTH_LU_S LISA_INVALID
#define LISA_XVFTINTRML_LU_S LISA_INVALID
#define LISA_XVFTINTRMH_LU_S LISA_INVALID
#define LISA_XVFTINTRPL_LU_S LISA_INVALID
#define LISA_XVFTINTRPH_LU_S LISA_INVALID
#define LISA_XVFTINTRZL_LU_S LISA_INVALID
#define LISA_XVFTINTRZH_LU_S LISA_INVALID
#define LISA_XVFTINTRNEL_LU_S LISA_INVALID
#define LISA_XVFTINTRNEH_LU_S LISA_INVALID

#define LISA_XVINSGR2VR_B LISA_INVALID
#define LISA_XVINSGR2VR_H LISA_INVALID
#define LISA_XVPICKVE2GR_B LISA_INVALID
#define LISA_XVPICKVE2GR_H LISA_INVALID
#define LISA_XVPICKVE2GR_BU LISA_INVALID
#define LISA_XVPICKVE2GR_HU LISA_INVALID
#define LISA_XVINSVE0_B LISA_INVALID
#define LISA_XVINSVE0_H LISA_INVALID
#define LISA_XVPICKVE_B LISA_INVALID
#define LISA_XVPICKVE_H LISA_INVALID
#define LISA_XVSLLWIL_Q_D LISA_INVALID
#define LISA_XVSLLWIL_QU_DU LISA_INVALID

IR2_OPCODE la_decode(uint32_t insn) {
    switch ((insn >> 26) & 0x3f) {
    case 0x0:
        /* 000000.. ........ ........ ........ */
        switch ((insn >> 22) & 0xf) {
        case 0x0:
            /* 00000000 00...... ........ ........ */
            switch ((insn >> 18) & 0xf) {
            case 0x0:
                /* 00000000 000000.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00000000 0....... ........ */
                    switch ((insn >> 10) & 0x1f) {
                    case 0x2:
                        /* 00000000 00000000 000010.. ........ */
                        switch ((insn >> 2) & 0x7) {
                        case 0x0:
                            /* 00000000 00000000 000010.. ...000.. */
                            /* la.decode:1 */
                            return LISA_GR2SCR;
                        }
                        return LISA_INVALID;
                    case 0x3:
                        /* 00000000 00000000 000011.. ........ */
                        switch ((insn >> 7) & 0x7) {
                        case 0x0:
                            /* 00000000 00000000 00001100 0....... */
                            /* la.decode:2 */
                            return LISA_SCR2GR;
                        }
                        return LISA_INVALID;
                    case 0x4:
                        /* 00000000 00000000 000100.. ........ */
                        /* la.decode:3 */
                        return LISA_CLO_W;
                    case 0x5:
                        /* 00000000 00000000 000101.. ........ */
                        /* la.decode:4 */
                        return LISA_CLZ_W;
                    case 0x6:
                        /* 00000000 00000000 000110.. ........ */
                        /* la.decode:5 */
                        return LISA_CTO_W;
                    case 0x7:
                        /* 00000000 00000000 000111.. ........ */
                        /* la.decode:6 */
                        return LISA_CTZ_W;
                    case 0x8:
                        /* 00000000 00000000 001000.. ........ */
                        /* la.decode:7 */
                        return LISA_CLO_D;
                    case 0x9:
                        /* 00000000 00000000 001001.. ........ */
                        /* la.decode:8 */
                        return LISA_CLZ_D;
                    case 0xa:
                        /* 00000000 00000000 001010.. ........ */
                        /* la.decode:9 */
                        return LISA_CTO_D;
                    case 0xb:
                        /* 00000000 00000000 001011.. ........ */
                        /* la.decode:10 */
                        return LISA_CTZ_D;
                    case 0xc:
                        /* 00000000 00000000 001100.. ........ */
                        /* la.decode:11 */
                        return LISA_REVB_2H;
                    case 0xd:
                        /* 00000000 00000000 001101.. ........ */
                        /* la.decode:12 */
                        return LISA_REVB_4H;
                    case 0xe:
                        /* 00000000 00000000 001110.. ........ */
                        /* la.decode:13 */
                        return LISA_REVB_2W;
                    case 0xf:
                        /* 00000000 00000000 001111.. ........ */
                        /* la.decode:14 */
                        return LISA_REVB_D;
                    case 0x10:
                        /* 00000000 00000000 010000.. ........ */
                        /* la.decode:15 */
                        return LISA_REVH_2W;
                    case 0x11:
                        /* 00000000 00000000 010001.. ........ */
                        /* la.decode:16 */
                        return LISA_REVH_D;
                    case 0x12:
                        /* 00000000 00000000 010010.. ........ */
                        /* la.decode:17 */
                        return LISA_BITREV_4B;
                    case 0x13:
                        /* 00000000 00000000 010011.. ........ */
                        /* la.decode:18 */
                        return LISA_BITREV_8B;
                    case 0x14:
                        /* 00000000 00000000 010100.. ........ */
                        /* la.decode:19 */
                        return LISA_BITREV_W;
                    case 0x15:
                        /* 00000000 00000000 010101.. ........ */
                        /* la.decode:20 */
                        return LISA_BITREV_D;
                    case 0x16:
                        /* 00000000 00000000 010110.. ........ */
                        /* la.decode:21 */
                        return LISA_EXT_W_H;
                    case 0x17:
                        /* 00000000 00000000 010111.. ........ */
                        /* la.decode:22 */
                        return LISA_EXT_W_B;
                    case 0x18:
                        /* 00000000 00000000 011000.. ........ */
                        /* la.decode:23 */
                        return LISA_RDTIMEL_W;
                    case 0x19:
                        /* 00000000 00000000 011001.. ........ */
                        /* la.decode:24 */
                        return LISA_RDTIMEH_W;
                    case 0x1a:
                        /* 00000000 00000000 011010.. ........ */
                        /* la.decode:25 */
                        return LISA_RDTIME_D;
                    case 0x1b:
                        /* 00000000 00000000 011011.. ........ */
                        /* la.decode:26 */
                        return LISA_CPUCFG;
                    case 0x1c:
                        /* 00000000 00000000 011100.. ........ */
                        switch (insn & 0x0000031f) {
                        case 0x00000000:
                            /* 00000000 00000000 01110000 ...00000 */
                            /* la.decode:27 */
                            return LISA_X86MTTOP;
                        }
                        return LISA_INVALID;
                    case 0x1d:
                        /* 00000000 00000000 011101.. ........ */
                        switch ((insn >> 5) & 0x1f) {
                        case 0x0:
                            /* 00000000 00000000 01110100 000..... */
                            /* la.decode:28 */
                            return LISA_X86MFTOP;
                        }
                        return LISA_INVALID;
                    case 0x1e:
                        /* 00000000 00000000 011110.. ........ */
                        /* la.decode:29 */
                        return LISA_X86LOOPE;
                    case 0x1f:
                        /* 00000000 00000000 011111.. ........ */
                        /* la.decode:30 */
                        return LISA_X86LOOPNE;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 00000000 00000000 1....... ........ */
                    switch (insn & 0x00007c1f) {
                    case 0x00000000:
                        /* 00000000 00000000 100000.. ...00000 */
                        /* la.decode:31 */
                        return LISA_X86INC_B;
                    case 0x00000001:
                        /* 00000000 00000000 100000.. ...00001 */
                        /* la.decode:32 */
                        return LISA_X86INC_H;
                    case 0x00000002:
                        /* 00000000 00000000 100000.. ...00010 */
                        /* la.decode:33 */
                        return LISA_X86INC_W;
                    case 0x00000003:
                        /* 00000000 00000000 100000.. ...00011 */
                        /* la.decode:34 */
                        return LISA_X86INC_D;
                    case 0x00000004:
                        /* 00000000 00000000 100000.. ...00100 */
                        /* la.decode:35 */
                        return LISA_X86DEC_B;
                    case 0x00000005:
                        /* 00000000 00000000 100000.. ...00101 */
                        /* la.decode:36 */
                        return LISA_X86DEC_H;
                    case 0x00000006:
                        /* 00000000 00000000 100000.. ...00110 */
                        /* la.decode:37 */
                        return LISA_X86DEC_W;
                    case 0x00000007:
                        /* 00000000 00000000 100000.. ...00111 */
                        /* la.decode:38 */
                        return LISA_X86DEC_D;
                    case 0x00000008:
                        /* 00000000 00000000 100000.. ...01000 */
                        switch ((insn >> 5) & 0x1f) {
                        case 0x0:
                            /* 00000000 00000000 10000000 00001000 */
                            /* la.decode:39 */
                            return LISA_X86SETTM;
                        case 0x1:
                            /* 00000000 00000000 10000000 00101000 */
                            /* la.decode:40 */
                            return LISA_X86CLRTM;
                        }
                        return LISA_INVALID;
                    case 0x00000009:
                        /* 00000000 00000000 100000.. ...01001 */
                        switch ((insn >> 5) & 0x1f) {
                        case 0x0:
                            /* 00000000 00000000 10000000 00001001 */
                            /* la.decode:41 */
                            return LISA_X86INCTOP;
                        case 0x1:
                            /* 00000000 00000000 10000000 00101001 */
                            /* la.decode:42 */
                            return LISA_X86DECTOP;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                case 0x2:
                    /* 00000000 00000001 0....... ........ */
                    switch (insn & 0x0000001f) {
                    case 0x00000000:
                        /* 00000000 00000001 0....... ...00000 */
                        /* la.decode:43 */
                        return LISA_ASRTLE_D;
                    }
                    return LISA_INVALID;
                case 0x3:
                    /* 00000000 00000001 1....... ........ */
                    switch (insn & 0x0000001f) {
                    case 0x00000000:
                        /* 00000000 00000001 1....... ...00000 */
                        /* la.decode:44 */
                        return LISA_ASRTGT_D;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00000000 000001.. ........ ........ */
                switch ((insn >> 17) & 0x1) {
                case 0x0:
                    /* 00000000 0000010. ........ ........ */
                    /* la.decode:45 */
                    return LISA_ALSL_W;
                case 0x1:
                    /* 00000000 0000011. ........ ........ */
                    /* la.decode:46 */
                    return LISA_ALSL_WU;
                }
                return LISA_INVALID;
            case 0x2:
                /* 00000000 000010.. ........ ........ */
                switch ((insn >> 17) & 0x1) {
                case 0x0:
                    /* 00000000 0000100. ........ ........ */
                    /* la.decode:47 */
                    return LISA_BYTEPICK_W;
                }
                return LISA_INVALID;
            case 0x3:
                /* 00000000 000011.. ........ ........ */
                /* la.decode:48 */
                return LISA_BYTEPICK_D;
            case 0x4:
                /* 00000000 000100.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00010000 0....... ........ */
                    /* la.decode:49 */
                    return LISA_ADD_W;
                case 0x1:
                    /* 00000000 00010000 1....... ........ */
                    /* la.decode:50 */
                    return LISA_ADD_D;
                case 0x2:
                    /* 00000000 00010001 0....... ........ */
                    /* la.decode:51 */
                    return LISA_SUB_W;
                case 0x3:
                    /* 00000000 00010001 1....... ........ */
                    /* la.decode:52 */
                    return LISA_SUB_D;
                case 0x4:
                    /* 00000000 00010010 0....... ........ */
                    /* la.decode:53 */
                    return LISA_SLT;
                case 0x5:
                    /* 00000000 00010010 1....... ........ */
                    /* la.decode:54 */
                    return LISA_SLTU;
                case 0x6:
                    /* 00000000 00010011 0....... ........ */
                    /* la.decode:55 */
                    return LISA_MASKEQZ;
                case 0x7:
                    /* 00000000 00010011 1....... ........ */
                    /* la.decode:56 */
                    return LISA_MASKNEZ;
                }
                return LISA_INVALID;
            case 0x5:
                /* 00000000 000101.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00010100 0....... ........ */
                    /* la.decode:57 */
                    return LISA_NOR;
                case 0x1:
                    /* 00000000 00010100 1....... ........ */
                    /* la.decode:58 */
                    return LISA_AND;
                case 0x2:
                    /* 00000000 00010101 0....... ........ */
                    /* la.decode:59 */
                    return LISA_OR;
                case 0x3:
                    /* 00000000 00010101 1....... ........ */
                    /* la.decode:60 */
                    return LISA_XOR;
                case 0x4:
                    /* 00000000 00010110 0....... ........ */
                    /* la.decode:61 */
                    return LISA_ORN;
                case 0x5:
                    /* 00000000 00010110 1....... ........ */
                    /* la.decode:62 */
                    return LISA_ANDN;
                case 0x6:
                    /* 00000000 00010111 0....... ........ */
                    /* la.decode:63 */
                    return LISA_SLL_W;
                case 0x7:
                    /* 00000000 00010111 1....... ........ */
                    /* la.decode:64 */
                    return LISA_SRL_W;
                }
                return LISA_INVALID;
            case 0x6:
                /* 00000000 000110.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00011000 0....... ........ */
                    /* la.decode:65 */
                    return LISA_SRA_W;
                case 0x1:
                    /* 00000000 00011000 1....... ........ */
                    /* la.decode:66 */
                    return LISA_SLL_D;
                case 0x2:
                    /* 00000000 00011001 0....... ........ */
                    /* la.decode:67 */
                    return LISA_SRL_D;
                case 0x3:
                    /* 00000000 00011001 1....... ........ */
                    /* la.decode:68 */
                    return LISA_SRA_D;
                case 0x4:
                    /* 00000000 00011010 0....... ........ */
                    /* la.decode:69 */
                    return LISA_ROTR_B;
                case 0x5:
                    /* 00000000 00011010 1....... ........ */
                    /* la.decode:70 */
                    return LISA_ROTR_H;
                case 0x6:
                    /* 00000000 00011011 0....... ........ */
                    /* la.decode:71 */
                    return LISA_ROTR_W;
                case 0x7:
                    /* 00000000 00011011 1....... ........ */
                    /* la.decode:72 */
                    return LISA_ROTR_D;
                }
                return LISA_INVALID;
            case 0x7:
                /* 00000000 000111.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00011100 0....... ........ */
                    /* la.decode:73 */
                    return LISA_MUL_W;
                case 0x1:
                    /* 00000000 00011100 1....... ........ */
                    /* la.decode:74 */
                    return LISA_MULH_W;
                case 0x2:
                    /* 00000000 00011101 0....... ........ */
                    /* la.decode:75 */
                    return LISA_MULH_WU;
                case 0x3:
                    /* 00000000 00011101 1....... ........ */
                    /* la.decode:76 */
                    return LISA_MUL_D;
                case 0x4:
                    /* 00000000 00011110 0....... ........ */
                    /* la.decode:77 */
                    return LISA_MULH_D;
                case 0x5:
                    /* 00000000 00011110 1....... ........ */
                    /* la.decode:78 */
                    return LISA_MULH_DU;
                case 0x6:
                    /* 00000000 00011111 0....... ........ */
                    /* la.decode:79 */
                    return LISA_MULW_D_W;
                case 0x7:
                    /* 00000000 00011111 1....... ........ */
                    /* la.decode:80 */
                    return LISA_MULW_D_WU;
                }
                return LISA_INVALID;
            case 0x8:
                /* 00000000 001000.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00100000 0....... ........ */
                    /* la.decode:81 */
                    return LISA_DIV_W;
                case 0x1:
                    /* 00000000 00100000 1....... ........ */
                    /* la.decode:82 */
                    return LISA_MOD_W;
                case 0x2:
                    /* 00000000 00100001 0....... ........ */
                    /* la.decode:83 */
                    return LISA_DIV_WU;
                case 0x3:
                    /* 00000000 00100001 1....... ........ */
                    /* la.decode:84 */
                    return LISA_MOD_WU;
                case 0x4:
                    /* 00000000 00100010 0....... ........ */
                    /* la.decode:85 */
                    return LISA_DIV_D;
                case 0x5:
                    /* 00000000 00100010 1....... ........ */
                    /* la.decode:86 */
                    return LISA_MOD_D;
                case 0x6:
                    /* 00000000 00100011 0....... ........ */
                    /* la.decode:87 */
                    return LISA_DIV_DU;
                case 0x7:
                    /* 00000000 00100011 1....... ........ */
                    /* la.decode:88 */
                    return LISA_MOD_DU;
                }
                return LISA_INVALID;
            case 0x9:
                /* 00000000 001001.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00100100 0....... ........ */
                    /* la.decode:89 */
                    return LISA_CRC_W_B_W;
                case 0x1:
                    /* 00000000 00100100 1....... ........ */
                    /* la.decode:90 */
                    return LISA_CRC_W_H_W;
                case 0x2:
                    /* 00000000 00100101 0....... ........ */
                    /* la.decode:91 */
                    return LISA_CRC_W_W_W;
                case 0x3:
                    /* 00000000 00100101 1....... ........ */
                    /* la.decode:92 */
                    return LISA_CRC_W_D_W;
                case 0x4:
                    /* 00000000 00100110 0....... ........ */
                    /* la.decode:93 */
                    return LISA_CRCC_W_B_W;
                case 0x5:
                    /* 00000000 00100110 1....... ........ */
                    /* la.decode:94 */
                    return LISA_CRCC_W_H_W;
                case 0x6:
                    /* 00000000 00100111 0....... ........ */
                    /* la.decode:95 */
                    return LISA_CRCC_W_W_W;
                case 0x7:
                    /* 00000000 00100111 1....... ........ */
                    /* la.decode:96 */
                    return LISA_CRCC_W_D_W;
                }
                return LISA_INVALID;
            case 0xa:
                /* 00000000 001010.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x2:
                    /* 00000000 00101001 0....... ........ */
                    /* la.decode:97 */
                    return LISA_ADDU12I_W;
                case 0x3:
                    /* 00000000 00101001 1....... ........ */
                    /* la.decode:98 */
                    return LISA_ADDU12I_D;
                case 0x4:
                    /* 00000000 00101010 0....... ........ */
                    /* la.decode:99 */
                    return LISA_BREAK;
                case 0x5:
                    /* 00000000 00101010 1....... ........ */
                    /* la.decode:100 */
                    return LISA_DBGCALL;
                case 0x6:
                    /* 00000000 00101011 0....... ........ */
                    /* la.decode:101 */
                    return LISA_SYSCALL;
                case 0x7:
                    /* 00000000 00101011 1....... ........ */
                    /* la.decode:102 */
                    return LISA_HYPCALL;
                }
                return LISA_INVALID;
            case 0xb:
                /* 00000000 001011.. ........ ........ */
                switch ((insn >> 17) & 0x1) {
                case 0x0:
                    /* 00000000 0010110. ........ ........ */
                    /* la.decode:103 */
                    return LISA_ALSL_D;
                }
                return LISA_INVALID;
            case 0xc:
                /* 00000000 001100.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00110000 0....... ........ */
                    /* la.decode:104 */
                    return LISA_ADC_B;
                case 0x1:
                    /* 00000000 00110000 1....... ........ */
                    /* la.decode:105 */
                    return LISA_ADC_H;
                case 0x2:
                    /* 00000000 00110001 0....... ........ */
                    /* la.decode:106 */
                    return LISA_ADC_W;
                case 0x3:
                    /* 00000000 00110001 1....... ........ */
                    /* la.decode:107 */
                    return LISA_ADC_D;
                case 0x4:
                    /* 00000000 00110010 0....... ........ */
                    /* la.decode:108 */
                    return LISA_SBC_B;
                case 0x5:
                    /* 00000000 00110010 1....... ........ */
                    /* la.decode:109 */
                    return LISA_SBC_H;
                case 0x6:
                    /* 00000000 00110011 0....... ........ */
                    /* la.decode:110 */
                    return LISA_SBC_W;
                case 0x7:
                    /* 00000000 00110011 1....... ........ */
                    /* la.decode:111 */
                    return LISA_SBC_D;
                }
                return LISA_INVALID;
            case 0xd:
                /* 00000000 001101.. ........ ........ */
                switch ((insn >> 15) & 0x7) {
                case 0x0:
                    /* 00000000 00110100 0....... ........ */
                    /* la.decode:112 */
                    return LISA_RCR_B;
                case 0x1:
                    /* 00000000 00110100 1....... ........ */
                    /* la.decode:113 */
                    return LISA_RCR_H;
                case 0x2:
                    /* 00000000 00110101 0....... ........ */
                    /* la.decode:114 */
                    return LISA_RCR_W;
                case 0x3:
                    /* 00000000 00110101 1....... ........ */
                    /* la.decode:115 */
                    return LISA_RCR_D;
                case 0x4:
                    /* 00000000 00110110 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x1:
                        /* 00000000 00110110 01...... ........ */
                        /* la.decode:116 */
                        return LISA_ARMMOVE;
                    }
                    return LISA_INVALID;
                case 0x5:
                    /* 00000000 00110110 1....... ........ */
                    switch (insn & 0x000043e0) {
                    case 0x00000000:
                        /* 00000000 00110110 10....00 000..... */
                        /* la.decode:117 */
                        return LISA_SETX86J;
                    case 0x00004000:
                        /* 00000000 00110110 11....00 000..... */
                        /* la.decode:118 */
                        return LISA_SETARMJ;
                    }
                    return LISA_INVALID;
                case 0x6:
                    /* 00000000 00110111 0....... ........ */
                    switch ((insn >> 4) & 0x1) {
                    case 0x1:
                        /* 00000000 00110111 0....... ...1.... */
                        /* la.decode:119 */
                        return LISA_ARMADD_W;
                    }
                    return LISA_INVALID;
                case 0x7:
                    /* 00000000 00110111 1....... ........ */
                    switch ((insn >> 4) & 0x1) {
                    case 0x1:
                        /* 00000000 00110111 1....... ...1.... */
                        /* la.decode:120 */
                        return LISA_ARMSUB_W;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            case 0xe:
                /* 00000000 001110.. ........ ........ */
                switch (insn & 0x00038010) {
                case 0x00000010:
                    /* 00000000 00111000 0....... ...1.... */
                    /* la.decode:121 */
                    return LISA_ARMADC_W;
                case 0x00008010:
                    /* 00000000 00111000 1....... ...1.... */
                    /* la.decode:122 */
                    return LISA_ARMSBC_W;
                case 0x00010010:
                    /* 00000000 00111001 0....... ...1.... */
                    /* la.decode:123 */
                    return LISA_ARMAND_W;
                case 0x00018010:
                    /* 00000000 00111001 1....... ...1.... */
                    /* la.decode:124 */
                    return LISA_ARMOR_W;
                case 0x00020010:
                    /* 00000000 00111010 0....... ...1.... */
                    /* la.decode:125 */
                    return LISA_ARMXOR_W;
                case 0x00028010:
                    /* 00000000 00111010 1....... ...1.... */
                    /* la.decode:126 */
                    return LISA_ARMSLL_W;
                case 0x00030010:
                    /* 00000000 00111011 0....... ...1.... */
                    /* la.decode:127 */
                    return LISA_ARMSRL_W;
                case 0x00038010:
                    /* 00000000 00111011 1....... ...1.... */
                    /* la.decode:128 */
                    return LISA_ARMSRA_W;
                }
                return LISA_INVALID;
            case 0xf:
                /* 00000000 001111.. ........ ........ */
                switch (insn & 0x00038010) {
                case 0x00000010:
                    /* 00000000 00111100 0....... ...1.... */
                    /* la.decode:129 */
                    return LISA_ARMROTR_W;
                case 0x00008010:
                    /* 00000000 00111100 1....... ...1.... */
                    /* la.decode:130 */
                    return LISA_ARMSLLI_W;
                case 0x00010010:
                    /* 00000000 00111101 0....... ...1.... */
                    /* la.decode:131 */
                    return LISA_ARMSRLI_W;
                case 0x00018010:
                    /* 00000000 00111101 1....... ...1.... */
                    /* la.decode:132 */
                    return LISA_ARMSRAI_W;
                case 0x00020010:
                    /* 00000000 00111110 0....... ...1.... */
                    /* la.decode:133 */
                    return LISA_ARMROTRI_W;
                case 0x00028000:
                    /* 00000000 00111110 1....... ...0.... */
                    switch (insn & 0x0000000f) {
                    case 0x00000000:
                        /* 00000000 00111110 1....... ...00000 */
                        /* la.decode:134 */
                        return LISA_X86MUL_B;
                    case 0x00000001:
                        /* 00000000 00111110 1....... ...00001 */
                        /* la.decode:135 */
                        return LISA_X86MUL_H;
                    case 0x00000002:
                        /* 00000000 00111110 1....... ...00010 */
                        /* la.decode:136 */
                        return LISA_X86MUL_W;
                    case 0x00000003:
                        /* 00000000 00111110 1....... ...00011 */
                        /* la.decode:137 */
                        return LISA_X86MUL_D;
                    case 0x00000004:
                        /* 00000000 00111110 1....... ...00100 */
                        /* la.decode:138 */
                        return LISA_X86MUL_BU;
                    case 0x00000005:
                        /* 00000000 00111110 1....... ...00101 */
                        /* la.decode:139 */
                        return LISA_X86MUL_HU;
                    case 0x00000006:
                        /* 00000000 00111110 1....... ...00110 */
                        /* la.decode:140 */
                        return LISA_X86MUL_WU;
                    case 0x00000007:
                        /* 00000000 00111110 1....... ...00111 */
                        /* la.decode:141 */
                        return LISA_X86MUL_DU;
                    }
                    return LISA_INVALID;
                case 0x00030000:
                    /* 00000000 00111111 0....... ...0.... */
                    switch (insn & 0x0000000f) {
                    case 0x00000000:
                        /* 00000000 00111111 0....... ...00000 */
                        /* la.decode:142 */
                        return LISA_X86ADD_WU;
                    case 0x00000001:
                        /* 00000000 00111111 0....... ...00001 */
                        /* la.decode:143 */
                        return LISA_X86ADD_DU;
                    case 0x00000002:
                        /* 00000000 00111111 0....... ...00010 */
                        /* la.decode:144 */
                        return LISA_X86SUB_WU;
                    case 0x00000003:
                        /* 00000000 00111111 0....... ...00011 */
                        /* la.decode:145 */
                        return LISA_X86SUB_DU;
                    case 0x00000004:
                        /* 00000000 00111111 0....... ...00100 */
                        /* la.decode:146 */
                        return LISA_X86ADD_B;
                    case 0x00000005:
                        /* 00000000 00111111 0....... ...00101 */
                        /* la.decode:147 */
                        return LISA_X86ADD_H;
                    case 0x00000006:
                        /* 00000000 00111111 0....... ...00110 */
                        /* la.decode:148 */
                        return LISA_X86ADD_W;
                    case 0x00000007:
                        /* 00000000 00111111 0....... ...00111 */
                        /* la.decode:149 */
                        return LISA_X86ADD_D;
                    case 0x00000008:
                        /* 00000000 00111111 0....... ...01000 */
                        /* la.decode:150 */
                        return LISA_X86SUB_B;
                    case 0x00000009:
                        /* 00000000 00111111 0....... ...01001 */
                        /* la.decode:151 */
                        return LISA_X86SUB_H;
                    case 0x0000000a:
                        /* 00000000 00111111 0....... ...01010 */
                        /* la.decode:152 */
                        return LISA_X86SUB_W;
                    case 0x0000000b:
                        /* 00000000 00111111 0....... ...01011 */
                        /* la.decode:153 */
                        return LISA_X86SUB_D;
                    case 0x0000000c:
                        /* 00000000 00111111 0....... ...01100 */
                        /* la.decode:154 */
                        return LISA_X86ADC_B;
                    case 0x0000000d:
                        /* 00000000 00111111 0....... ...01101 */
                        /* la.decode:155 */
                        return LISA_X86ADC_H;
                    case 0x0000000e:
                        /* 00000000 00111111 0....... ...01110 */
                        /* la.decode:156 */
                        return LISA_X86ADC_W;
                    case 0x0000000f:
                        /* 00000000 00111111 0....... ...01111 */
                        /* la.decode:157 */
                        return LISA_X86ADC_D;
                    }
                    return LISA_INVALID;
                case 0x00030010:
                    /* 00000000 00111111 0....... ...1.... */
                    switch (insn & 0x0000000f) {
                    case 0x00000000:
                        /* 00000000 00111111 0....... ...10000 */
                        /* la.decode:158 */
                        return LISA_X86SBC_B;
                    case 0x00000001:
                        /* 00000000 00111111 0....... ...10001 */
                        /* la.decode:159 */
                        return LISA_X86SBC_H;
                    case 0x00000002:
                        /* 00000000 00111111 0....... ...10010 */
                        /* la.decode:160 */
                        return LISA_X86SBC_W;
                    case 0x00000003:
                        /* 00000000 00111111 0....... ...10011 */
                        /* la.decode:161 */
                        return LISA_X86SBC_D;
                    case 0x00000004:
                        /* 00000000 00111111 0....... ...10100 */
                        /* la.decode:162 */
                        return LISA_X86SLL_B;
                    case 0x00000005:
                        /* 00000000 00111111 0....... ...10101 */
                        /* la.decode:163 */
                        return LISA_X86SLL_H;
                    case 0x00000006:
                        /* 00000000 00111111 0....... ...10110 */
                        /* la.decode:164 */
                        return LISA_X86SLL_W;
                    case 0x00000007:
                        /* 00000000 00111111 0....... ...10111 */
                        /* la.decode:165 */
                        return LISA_X86SLL_D;
                    case 0x00000008:
                        /* 00000000 00111111 0....... ...11000 */
                        /* la.decode:166 */
                        return LISA_X86SRL_B;
                    case 0x00000009:
                        /* 00000000 00111111 0....... ...11001 */
                        /* la.decode:167 */
                        return LISA_X86SRL_H;
                    case 0x0000000a:
                        /* 00000000 00111111 0....... ...11010 */
                        /* la.decode:168 */
                        return LISA_X86SRL_W;
                    case 0x0000000b:
                        /* 00000000 00111111 0....... ...11011 */
                        /* la.decode:169 */
                        return LISA_X86SRL_D;
                    case 0x0000000c:
                        /* 00000000 00111111 0....... ...11100 */
                        /* la.decode:170 */
                        return LISA_X86SRA_B;
                    case 0x0000000d:
                        /* 00000000 00111111 0....... ...11101 */
                        /* la.decode:171 */
                        return LISA_X86SRA_H;
                    case 0x0000000e:
                        /* 00000000 00111111 0....... ...11110 */
                        /* la.decode:172 */
                        return LISA_X86SRA_W;
                    case 0x0000000f:
                        /* 00000000 00111111 0....... ...11111 */
                        /* la.decode:173 */
                        return LISA_X86SRA_D;
                    }
                    return LISA_INVALID;
                case 0x00038000:
                    /* 00000000 00111111 1....... ...0.... */
                    switch (insn & 0x0000000f) {
                    case 0x00000000:
                        /* 00000000 00111111 1....... ...00000 */
                        /* la.decode:174 */
                        return LISA_X86ROTR_B;
                    case 0x00000001:
                        /* 00000000 00111111 1....... ...00001 */
                        /* la.decode:175 */
                        return LISA_X86ROTR_H;
                    case 0x00000002:
                        /* 00000000 00111111 1....... ...00010 */
                        /* la.decode:176 */
                        return LISA_X86ROTR_D;
                    case 0x00000003:
                        /* 00000000 00111111 1....... ...00011 */
                        /* la.decode:177 */
                        return LISA_X86ROTR_W;
                    case 0x00000004:
                        /* 00000000 00111111 1....... ...00100 */
                        /* la.decode:178 */
                        return LISA_X86ROTL_B;
                    case 0x00000005:
                        /* 00000000 00111111 1....... ...00101 */
                        /* la.decode:179 */
                        return LISA_X86ROTL_H;
                    case 0x00000006:
                        /* 00000000 00111111 1....... ...00110 */
                        /* la.decode:180 */
                        return LISA_X86ROTL_W;
                    case 0x00000007:
                        /* 00000000 00111111 1....... ...00111 */
                        /* la.decode:181 */
                        return LISA_X86ROTL_D;
                    case 0x00000008:
                        /* 00000000 00111111 1....... ...01000 */
                        /* la.decode:182 */
                        return LISA_X86RCR_B;
                    case 0x00000009:
                        /* 00000000 00111111 1....... ...01001 */
                        /* la.decode:183 */
                        return LISA_X86RCR_H;
                    case 0x0000000a:
                        /* 00000000 00111111 1....... ...01010 */
                        /* la.decode:184 */
                        return LISA_X86RCR_W;
                    case 0x0000000b:
                        /* 00000000 00111111 1....... ...01011 */
                        /* la.decode:185 */
                        return LISA_X86RCR_D;
                    case 0x0000000c:
                        /* 00000000 00111111 1....... ...01100 */
                        /* la.decode:186 */
                        return LISA_X86RCL_B;
                    case 0x0000000d:
                        /* 00000000 00111111 1....... ...01101 */
                        /* la.decode:187 */
                        return LISA_X86RCL_H;
                    case 0x0000000e:
                        /* 00000000 00111111 1....... ...01110 */
                        /* la.decode:188 */
                        return LISA_X86RCL_W;
                    case 0x0000000f:
                        /* 00000000 00111111 1....... ...01111 */
                        /* la.decode:189 */
                        return LISA_X86RCL_D;
                    }
                    return LISA_INVALID;
                case 0x00038010:
                    /* 00000000 00111111 1....... ...1.... */
                    switch (insn & 0x0000000f) {
                    case 0x00000000:
                        /* 00000000 00111111 1....... ...10000 */
                        /* la.decode:190 */
                        return LISA_X86AND_B;
                    case 0x00000001:
                        /* 00000000 00111111 1....... ...10001 */
                        /* la.decode:191 */
                        return LISA_X86AND_H;
                    case 0x00000002:
                        /* 00000000 00111111 1....... ...10010 */
                        /* la.decode:192 */
                        return LISA_X86AND_W;
                    case 0x00000003:
                        /* 00000000 00111111 1....... ...10011 */
                        /* la.decode:193 */
                        return LISA_X86AND_D;
                    case 0x00000004:
                        /* 00000000 00111111 1....... ...10100 */
                        /* la.decode:194 */
                        return LISA_X86OR_B;
                    case 0x00000005:
                        /* 00000000 00111111 1....... ...10101 */
                        /* la.decode:195 */
                        return LISA_X86OR_H;
                    case 0x00000006:
                        /* 00000000 00111111 1....... ...10110 */
                        /* la.decode:196 */
                        return LISA_X86OR_W;
                    case 0x00000007:
                        /* 00000000 00111111 1....... ...10111 */
                        /* la.decode:197 */
                        return LISA_X86OR_D;
                    case 0x00000008:
                        /* 00000000 00111111 1....... ...11000 */
                        /* la.decode:198 */
                        return LISA_X86XOR_B;
                    case 0x00000009:
                        /* 00000000 00111111 1....... ...11001 */
                        /* la.decode:199 */
                        return LISA_X86XOR_H;
                    case 0x0000000a:
                        /* 00000000 00111111 1....... ...11010 */
                        /* la.decode:200 */
                        return LISA_X86XOR_W;
                    case 0x0000000b:
                        /* 00000000 00111111 1....... ...11011 */
                        /* la.decode:201 */
                        return LISA_X86XOR_D;
                    case 0x0000000c:
                        /* 00000000 00111111 1....... ...11100 */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 00000000 00111111 11...... ...11100 */
                            /* la.decode:202 */
                            return LISA_ARMNOT_W;
                        }
                        return LISA_INVALID;
                    case 0x0000000d:
                        /* 00000000 00111111 1....... ...11101 */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 00000000 00111111 11...... ...11101 */
                            /* la.decode:203 */
                            return LISA_ARMMOV_W;
                        }
                        return LISA_INVALID;
                    case 0x0000000e:
                        /* 00000000 00111111 1....... ...11110 */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 00000000 00111111 11...... ...11110 */
                            /* la.decode:204 */
                            return LISA_ARMMOV_D;
                        }
                        return LISA_INVALID;
                    case 0x0000000f:
                        /* 00000000 00111111 1....... ...11111 */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 00000000 00111111 11...... ...11111 */
                            /* la.decode:205 */
                            return LISA_ARMRRX_W;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0x1:
            /* 00000000 01...... ........ ........ */
            switch ((insn >> 21) & 0x1) {
            case 0x0:
                /* 00000000 010..... ........ ........ */
                switch ((insn >> 18) & 0x7) {
                case 0x0:
                    /* 00000000 010000.. ........ ........ */
                    switch ((insn >> 16) & 0x3) {
                    case 0x0:
                        /* 00000000 01000000 ........ ........ */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01000000 1....... ........ */
                            /* la.decode:206 */
                            return LISA_SLLI_W;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000000 01000001 ........ ........ */
                        /* la.decode:207 */
                        return LISA_SLLI_D;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 00000000 010001.. ........ ........ */
                    switch ((insn >> 16) & 0x3) {
                    case 0x0:
                        /* 00000000 01000100 ........ ........ */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01000100 1....... ........ */
                            /* la.decode:208 */
                            return LISA_SRLI_W;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000000 01000101 ........ ........ */
                        /* la.decode:209 */
                        return LISA_SRLI_D;
                    }
                    return LISA_INVALID;
                case 0x2:
                    /* 00000000 010010.. ........ ........ */
                    switch ((insn >> 16) & 0x3) {
                    case 0x0:
                        /* 00000000 01001000 ........ ........ */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01001000 1....... ........ */
                            /* la.decode:210 */
                            return LISA_SRAI_W;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000000 01001001 ........ ........ */
                        /* la.decode:211 */
                        return LISA_SRAI_D;
                    }
                    return LISA_INVALID;
                case 0x3:
                    /* 00000000 010011.. ........ ........ */
                    switch ((insn >> 16) & 0x3) {
                    case 0x0:
                        /* 00000000 01001100 ........ ........ */
                        switch ((insn >> 15) & 0x1) {
                        case 0x0:
                            /* 00000000 01001100 0....... ........ */
                            switch ((insn >> 14) & 0x1) {
                            case 0x0:
                                /* 00000000 01001100 00...... ........ */
                                switch ((insn >> 13) & 0x1) {
                                case 0x1:
                                    /* 00000000 01001100 001..... ........ */
                                    /* la.decode:212 */
                                    return LISA_ROTRI_B;
                                }
                                return LISA_INVALID;
                            case 0x1:
                                /* 00000000 01001100 01...... ........ */
                                /* la.decode:213 */
                                return LISA_ROTRI_H;
                            }
                            return LISA_INVALID;
                        case 0x1:
                            /* 00000000 01001100 1....... ........ */
                            /* la.decode:214 */
                            return LISA_ROTRI_W;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000000 01001101 ........ ........ */
                        /* la.decode:215 */
                        return LISA_ROTRI_D;
                    }
                    return LISA_INVALID;
                case 0x4:
                    /* 00000000 010100.. ........ ........ */
                    switch ((insn >> 16) & 0x3) {
                    case 0x0:
                        /* 00000000 01010000 ........ ........ */
                        switch ((insn >> 15) & 0x1) {
                        case 0x0:
                            /* 00000000 01010000 0....... ........ */
                            switch ((insn >> 14) & 0x1) {
                            case 0x0:
                                /* 00000000 01010000 00...... ........ */
                                switch ((insn >> 13) & 0x1) {
                                case 0x1:
                                    /* 00000000 01010000 001..... ........ */
                                    /* la.decode:216 */
                                    return LISA_RCRI_B;
                                }
                                return LISA_INVALID;
                            case 0x1:
                                /* 00000000 01010000 01...... ........ */
                                /* la.decode:217 */
                                return LISA_RCRI_H;
                            }
                            return LISA_INVALID;
                        case 0x1:
                            /* 00000000 01010000 1....... ........ */
                            /* la.decode:218 */
                            return LISA_RCRI_W;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000000 01010001 ........ ........ */
                        /* la.decode:219 */
                        return LISA_RCRI_D;
                    }
                    return LISA_INVALID;
                case 0x5:
                    /* 00000000 010101.. ........ ........ */
                    switch (insn & 0x0003001f) {
                    case 0x00000000:
                        /* 00000000 01010100 ........ ...00000 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...00000 */
                            /* la.decode:220 */
                            return LISA_X86SLLI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000001:
                        /* 00000000 01010100 ........ ...00001 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...00001 */
                            /* la.decode:221 */
                            return LISA_X86SLLI_H;
                        }
                        return LISA_INVALID;
                    case 0x00000002:
                        /* 00000000 01010100 ........ ...00010 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...00010 */
                            /* la.decode:222 */
                            return LISA_X86SLLI_W;
                        }
                        return LISA_INVALID;
                    case 0x00000004:
                        /* 00000000 01010100 ........ ...00100 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...00100 */
                            /* la.decode:224 */
                            return LISA_X86SRLI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000005:
                        /* 00000000 01010100 ........ ...00101 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...00101 */
                            /* la.decode:225 */
                            return LISA_X86SRLI_H;
                        }
                        return LISA_INVALID;
                    case 0x00000006:
                        /* 00000000 01010100 ........ ...00110 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...00110 */
                            /* la.decode:226 */
                            return LISA_X86SRLI_W;
                        }
                        return LISA_INVALID;
                    case 0x00000008:
                        /* 00000000 01010100 ........ ...01000 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...01000 */
                            /* la.decode:228 */
                            return LISA_X86SRAI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000009:
                        /* 00000000 01010100 ........ ...01001 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...01001 */
                            /* la.decode:229 */
                            return LISA_X86SRAI_H;
                        }
                        return LISA_INVALID;
                    case 0x0000000a:
                        /* 00000000 01010100 ........ ...01010 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...01010 */
                            /* la.decode:230 */
                            return LISA_X86SRAI_W;
                        }
                        return LISA_INVALID;
                    case 0x0000000c:
                        /* 00000000 01010100 ........ ...01100 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...01100 */
                            /* la.decode:232 */
                            return LISA_X86ROTRI_B;
                        }
                        return LISA_INVALID;
                    case 0x0000000d:
                        /* 00000000 01010100 ........ ...01101 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...01101 */
                            /* la.decode:233 */
                            return LISA_X86ROTRI_H;
                        }
                        return LISA_INVALID;
                    case 0x0000000e:
                        /* 00000000 01010100 ........ ...01110 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...01110 */
                            /* la.decode:234 */
                            return LISA_X86ROTRI_W;
                        }
                        return LISA_INVALID;
                    case 0x00000010:
                        /* 00000000 01010100 ........ ...10000 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...10000 */
                            /* la.decode:236 */
                            return LISA_X86RCRI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000011:
                        /* 00000000 01010100 ........ ...10001 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...10001 */
                            /* la.decode:237 */
                            return LISA_X86RCRI_H;
                        }
                        return LISA_INVALID;
                    case 0x00000012:
                        /* 00000000 01010100 ........ ...10010 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...10010 */
                            /* la.decode:238 */
                            return LISA_X86RCRI_W;
                        }
                        return LISA_INVALID;
                    case 0x00000014:
                        /* 00000000 01010100 ........ ...10100 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...10100 */
                            /* la.decode:240 */
                            return LISA_X86ROTLI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000015:
                        /* 00000000 01010100 ........ ...10101 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...10101 */
                            /* la.decode:241 */
                            return LISA_X86ROTLI_H;
                        }
                        return LISA_INVALID;
                    case 0x00000016:
                        /* 00000000 01010100 ........ ...10110 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...10110 */
                            /* la.decode:242 */
                            return LISA_X86ROTLI_W;
                        }
                        return LISA_INVALID;
                    case 0x00000018:
                        /* 00000000 01010100 ........ ...11000 */
                        switch ((insn >> 13) & 0x7) {
                        case 0x1:
                            /* 00000000 01010100 001..... ...11000 */
                            /* la.decode:244 */
                            return LISA_X86RCLI_B;
                        }
                        return LISA_INVALID;
                    case 0x00000019:
                        /* 00000000 01010100 ........ ...11001 */
                        switch ((insn >> 14) & 0x3) {
                        case 0x1:
                            /* 00000000 01010100 01...... ...11001 */
                            /* la.decode:245 */
                            return LISA_X86RCLI_H;
                        }
                        return LISA_INVALID;
                    case 0x0000001a:
                        /* 00000000 01010100 ........ ...11010 */
                        switch ((insn >> 15) & 0x1) {
                        case 0x1:
                            /* 00000000 01010100 1....... ...11010 */
                            /* la.decode:246 */
                            return LISA_X86RCLI_W;
                        }
                        return LISA_INVALID;
                    case 0x00010003:
                        /* 00000000 01010101 ........ ...00011 */
                        /* la.decode:223 */
                        return LISA_X86SLLI_D;
                    case 0x00010007:
                        /* 00000000 01010101 ........ ...00111 */
                        /* la.decode:227 */
                        return LISA_X86SRLI_D;
                    case 0x0001000b:
                        /* 00000000 01010101 ........ ...01011 */
                        /* la.decode:231 */
                        return LISA_X86SRAI_D;
                    case 0x0001000f:
                        /* 00000000 01010101 ........ ...01111 */
                        /* la.decode:235 */
                        return LISA_X86ROTRI_D;
                    case 0x00010013:
                        /* 00000000 01010101 ........ ...10011 */
                        /* la.decode:239 */
                        return LISA_X86RCRI_D;
                    case 0x00010017:
                        /* 00000000 01010101 ........ ...10111 */
                        /* la.decode:243 */
                        return LISA_X86ROTLI_D;
                    case 0x0001001b:
                        /* 00000000 01010101 ........ ...11011 */
                        /* la.decode:247 */
                        return LISA_X86RCLI_D;
                    }
                    return LISA_INVALID;
                case 0x6:
                    /* 00000000 010110.. ........ ........ */
                    /* la.decode:248 */
                    return LISA_X86SETTAG;
                case 0x7:
                    /* 00000000 010111.. ........ ........ */
                    switch ((insn >> 5) & 0x1f) {
                    case 0x0:
                        /* 00000000 010111.. ......00 000..... */
                        /* la.decode:249 */
                        return LISA_X86MFFLAG;
                    case 0x1:
                        /* 00000000 010111.. ......00 001..... */
                        /* la.decode:250 */
                        return LISA_X86MTFLAG;
                    case 0x2:
                        /* 00000000 010111.. ......00 010..... */
                        /* la.decode:251 */
                        return LISA_ARMMFFLAG;
                    case 0x3:
                        /* 00000000 010111.. ......00 011..... */
                        /* la.decode:252 */
                        return LISA_ARMMTFLAG;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00000000 011..... ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 00000000 011..... 0....... ........ */
                    /* la.decode:253 */
                    return LISA_BSTRINS_W;
                case 0x1:
                    /* 00000000 011..... 1....... ........ */
                    /* la.decode:254 */
                    return LISA_BSTRPICK_W;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0x2:
            /* 00000000 10...... ........ ........ */
            /* la.decode:255 */
            return LISA_BSTRINS_D;
        case 0x3:
            /* 00000000 11...... ........ ........ */
            /* la.decode:256 */
            return LISA_BSTRPICK_D;
        case 0x4:
            /* 00000001 00...... ........ ........ */
            switch ((insn >> 15) & 0x7f) {
            case 0x0:
                /* 00000001 00000000 0....... ........ */
                /* la.decode:257 */
                return LISA_FADD_H;
            case 0x1:
                /* 00000001 00000000 1....... ........ */
                /* la.decode:258 */
                return LISA_FADD_S;
            case 0x2:
                /* 00000001 00000001 0....... ........ */
                /* la.decode:259 */
                return LISA_FADD_D;
            case 0x4:
                /* 00000001 00000010 0....... ........ */
                /* la.decode:260 */
                return LISA_FSUB_H;
            case 0x5:
                /* 00000001 00000010 1....... ........ */
                /* la.decode:261 */
                return LISA_FSUB_S;
            case 0x6:
                /* 00000001 00000011 0....... ........ */
                /* la.decode:262 */
                return LISA_FSUB_D;
            case 0x8:
                /* 00000001 00000100 0....... ........ */
                /* la.decode:263 */
                return LISA_FMUL_H;
            case 0x9:
                /* 00000001 00000100 1....... ........ */
                /* la.decode:264 */
                return LISA_FMUL_S;
            case 0xa:
                /* 00000001 00000101 0....... ........ */
                /* la.decode:265 */
                return LISA_FMUL_D;
            case 0xc:
                /* 00000001 00000110 0....... ........ */
                /* la.decode:266 */
                return LISA_FDIV_H;
            case 0xd:
                /* 00000001 00000110 1....... ........ */
                /* la.decode:267 */
                return LISA_FDIV_S;
            case 0xe:
                /* 00000001 00000111 0....... ........ */
                /* la.decode:268 */
                return LISA_FDIV_D;
            case 0x10:
                /* 00000001 00001000 0....... ........ */
                /* la.decode:269 */
                return LISA_FMAX_H;
            case 0x11:
                /* 00000001 00001000 1....... ........ */
                /* la.decode:270 */
                return LISA_FMAX_S;
            case 0x12:
                /* 00000001 00001001 0....... ........ */
                /* la.decode:271 */
                return LISA_FMAX_D;
            case 0x14:
                /* 00000001 00001010 0....... ........ */
                /* la.decode:272 */
                return LISA_FMIN_H;
            case 0x15:
                /* 00000001 00001010 1....... ........ */
                /* la.decode:273 */
                return LISA_FMIN_S;
            case 0x16:
                /* 00000001 00001011 0....... ........ */
                /* la.decode:274 */
                return LISA_FMIN_D;
            case 0x18:
                /* 00000001 00001100 0....... ........ */
                /* la.decode:275 */
                return LISA_FMAXA_H;
            case 0x19:
                /* 00000001 00001100 1....... ........ */
                /* la.decode:276 */
                return LISA_FMAXA_S;
            case 0x1a:
                /* 00000001 00001101 0....... ........ */
                /* la.decode:277 */
                return LISA_FMAXA_D;
            case 0x1c:
                /* 00000001 00001110 0....... ........ */
                /* la.decode:278 */
                return LISA_FMINA_H;
            case 0x1d:
                /* 00000001 00001110 1....... ........ */
                /* la.decode:279 */
                return LISA_FMINA_S;
            case 0x1e:
                /* 00000001 00001111 0....... ........ */
                /* la.decode:280 */
                return LISA_FMINA_D;
            case 0x20:
                /* 00000001 00010000 0....... ........ */
                /* la.decode:281 */
                return LISA_FSCALEB_H;
            case 0x21:
                /* 00000001 00010000 1....... ........ */
                /* la.decode:282 */
                return LISA_FSCALEB_S;
            case 0x22:
                /* 00000001 00010001 0....... ........ */
                /* la.decode:283 */
                return LISA_FSCALEB_D;
            case 0x24:
                /* 00000001 00010010 0....... ........ */
                /* la.decode:284 */
                return LISA_FCOPYSIGN_H;
            case 0x25:
                /* 00000001 00010010 1....... ........ */
                /* la.decode:285 */
                return LISA_FCOPYSIGN_S;
            case 0x26:
                /* 00000001 00010011 0....... ........ */
                /* la.decode:286 */
                return LISA_FCOPYSIGN_D;
            case 0x28:
                /* 00000001 00010100 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00010100 000000.. ........ */
                    /* la.decode:287 */
                    return LISA_FABS_H;
                case 0x1:
                    /* 00000001 00010100 000001.. ........ */
                    /* la.decode:288 */
                    return LISA_FABS_S;
                case 0x2:
                    /* 00000001 00010100 000010.. ........ */
                    /* la.decode:289 */
                    return LISA_FABS_D;
                case 0x4:
                    /* 00000001 00010100 000100.. ........ */
                    /* la.decode:290 */
                    return LISA_FNEG_H;
                case 0x5:
                    /* 00000001 00010100 000101.. ........ */
                    /* la.decode:291 */
                    return LISA_FNEG_S;
                case 0x6:
                    /* 00000001 00010100 000110.. ........ */
                    /* la.decode:292 */
                    return LISA_FNEG_D;
                case 0x8:
                    /* 00000001 00010100 001000.. ........ */
                    /* la.decode:293 */
                    return LISA_FLOGB_H;
                case 0x9:
                    /* 00000001 00010100 001001.. ........ */
                    /* la.decode:294 */
                    return LISA_FLOGB_S;
                case 0xa:
                    /* 00000001 00010100 001010.. ........ */
                    /* la.decode:295 */
                    return LISA_FLOGB_D;
                case 0xc:
                    /* 00000001 00010100 001100.. ........ */
                    /* la.decode:296 */
                    return LISA_FCLASS_H;
                case 0xd:
                    /* 00000001 00010100 001101.. ........ */
                    /* la.decode:297 */
                    return LISA_FCLASS_S;
                case 0xe:
                    /* 00000001 00010100 001110.. ........ */
                    /* la.decode:298 */
                    return LISA_FCLASS_D;
                case 0x10:
                    /* 00000001 00010100 010000.. ........ */
                    /* la.decode:299 */
                    return LISA_FSQRT_H;
                case 0x11:
                    /* 00000001 00010100 010001.. ........ */
                    /* la.decode:300 */
                    return LISA_FSQRT_S;
                case 0x12:
                    /* 00000001 00010100 010010.. ........ */
                    /* la.decode:301 */
                    return LISA_FSQRT_D;
                case 0x14:
                    /* 00000001 00010100 010100.. ........ */
                    /* la.decode:302 */
                    return LISA_FRECIP_H;
                case 0x15:
                    /* 00000001 00010100 010101.. ........ */
                    /* la.decode:303 */
                    return LISA_FRECIP_S;
                case 0x16:
                    /* 00000001 00010100 010110.. ........ */
                    /* la.decode:304 */
                    return LISA_FRECIP_D;
                case 0x18:
                    /* 00000001 00010100 011000.. ........ */
                    /* la.decode:305 */
                    return LISA_FRSQRT_H;
                case 0x19:
                    /* 00000001 00010100 011001.. ........ */
                    /* la.decode:306 */
                    return LISA_FRSQRT_S;
                case 0x1a:
                    /* 00000001 00010100 011010.. ........ */
                    /* la.decode:307 */
                    return LISA_FRSQRT_D;
                case 0x1c:
                    /* 00000001 00010100 011100.. ........ */
                    /* la.decode:308 */
                    return LISA_FRECIPE_H;
                case 0x1d:
                    /* 00000001 00010100 011101.. ........ */
                    /* la.decode:309 */
                    return LISA_FRECIPE_S;
                case 0x1e:
                    /* 00000001 00010100 011110.. ........ */
                    /* la.decode:310 */
                    return LISA_FRECIPE_D;
                }
                return LISA_INVALID;
            case 0x29:
                /* 00000001 00010100 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00010100 100000.. ........ */
                    /* la.decode:311 */
                    return LISA_FRSQRTE_H;
                case 0x1:
                    /* 00000001 00010100 100001.. ........ */
                    /* la.decode:312 */
                    return LISA_FRSQRTE_S;
                case 0x2:
                    /* 00000001 00010100 100010.. ........ */
                    /* la.decode:313 */
                    return LISA_FRSQRTE_D;
                case 0x4:
                    /* 00000001 00010100 100100.. ........ */
                    /* la.decode:314 */
                    return LISA_FMOV_H;
                case 0x5:
                    /* 00000001 00010100 100101.. ........ */
                    /* la.decode:315 */
                    return LISA_FMOV_S;
                case 0x6:
                    /* 00000001 00010100 100110.. ........ */
                    /* la.decode:316 */
                    return LISA_FMOV_D;
                case 0x8:
                    /* 00000001 00010100 101000.. ........ */
                    /* la.decode:317 */
                    return LISA_MOVGR2FR_H;
                case 0x9:
                    /* 00000001 00010100 101001.. ........ */
                    /* la.decode:318 */
                    return LISA_MOVGR2FR_W;
                case 0xa:
                    /* 00000001 00010100 101010.. ........ */
                    /* la.decode:319 */
                    return LISA_MOVGR2FR_D;
                case 0xb:
                    /* 00000001 00010100 101011.. ........ */
                    /* la.decode:320 */
                    return LISA_MOVGR2FRH_W;
                case 0xc:
                    /* 00000001 00010100 101100.. ........ */
                    /* la.decode:321 */
                    return LISA_MOVFR2GR_H;
                case 0xd:
                    /* 00000001 00010100 101101.. ........ */
                    /* la.decode:322 */
                    return LISA_MOVFR2GR_S;
                case 0xe:
                    /* 00000001 00010100 101110.. ........ */
                    /* la.decode:323 */
                    return LISA_MOVFR2GR_D;
                case 0xf:
                    /* 00000001 00010100 101111.. ........ */
                    /* la.decode:324 */
                    return LISA_MOVFRH2GR_S;
                case 0x10:
                    /* 00000001 00010100 110000.. ........ */
                    /* la.decode:325 */
                    return LISA_MOVGR2FCSR;
                case 0x12:
                    /* 00000001 00010100 110010.. ........ */
                    /* la.decode:326 */
                    return LISA_MOVFCSR2GR;
                case 0x14:
                    /* 00000001 00010100 110100.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 00000001 00010100 110100.. ...00... */
                        /* la.decode:327 */
                        return LISA_MOVFR2CF;
                    }
                    return LISA_INVALID;
                case 0x15:
                    /* 00000001 00010100 110101.. ........ */
                    switch ((insn >> 8) & 0x3) {
                    case 0x0:
                        /* 00000001 00010100 11010100 ........ */
                        /* la.decode:328 */
                        return LISA_MOVCF2FR;
                    }
                    return LISA_INVALID;
                case 0x16:
                    /* 00000001 00010100 110110.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 00000001 00010100 110110.. ...00... */
                        /* la.decode:329 */
                        return LISA_MOVGR2CF;
                    }
                    return LISA_INVALID;
                case 0x17:
                    /* 00000001 00010100 110111.. ........ */
                    switch ((insn >> 8) & 0x3) {
                    case 0x0:
                        /* 00000001 00010100 11011100 ........ */
                        /* la.decode:330 */
                        return LISA_MOVCF2GR;
                    }
                    return LISA_INVALID;
                case 0x18:
                    /* 00000001 00010100 111000.. ........ */
                    /* la.decode:331 */
                    return LISA_FCVT_LD_D;
                case 0x19:
                    /* 00000001 00010100 111001.. ........ */
                    /* la.decode:332 */
                    return LISA_FCVT_UD_D;
                }
                return LISA_INVALID;
            case 0x2a:
                /* 00000001 00010101 0....... ........ */
                /* la.decode:333 */
                return LISA_FCVT_D_LD;
            case 0x30:
                /* 00000001 00011000 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x1:
                    /* 00000001 00011000 000001.. ........ */
                    /* la.decode:334 */
                    return LISA_FCVTRM_H_S;
                case 0x2:
                    /* 00000001 00011000 000010.. ........ */
                    /* la.decode:335 */
                    return LISA_FCVTRM_H_D;
                case 0x3:
                    /* 00000001 00011000 000011.. ........ */
                    /* la.decode:336 */
                    return LISA_FCVTRM_H_Q;
                case 0x4:
                    /* 00000001 00011000 000100.. ........ */
                    /* la.decode:337 */
                    return LISA_FCVTRM_S_H;
                case 0x6:
                    /* 00000001 00011000 000110.. ........ */
                    /* la.decode:338 */
                    return LISA_FCVTRM_S_D;
                case 0x7:
                    /* 00000001 00011000 000111.. ........ */
                    /* la.decode:339 */
                    return LISA_FCVTRM_S_Q;
                case 0x8:
                    /* 00000001 00011000 001000.. ........ */
                    /* la.decode:340 */
                    return LISA_FCVTRM_D_H;
                case 0x9:
                    /* 00000001 00011000 001001.. ........ */
                    /* la.decode:341 */
                    return LISA_FCVTRM_D_S;
                case 0xb:
                    /* 00000001 00011000 001011.. ........ */
                    /* la.decode:342 */
                    return LISA_FCVTRM_D_Q;
                case 0xc:
                    /* 00000001 00011000 001100.. ........ */
                    /* la.decode:343 */
                    return LISA_FCVTRM_Q_H;
                case 0xd:
                    /* 00000001 00011000 001101.. ........ */
                    /* la.decode:344 */
                    return LISA_FCVTRM_Q_S;
                case 0xe:
                    /* 00000001 00011000 001110.. ........ */
                    /* la.decode:345 */
                    return LISA_FCVTRM_Q_D;
                case 0x11:
                    /* 00000001 00011000 010001.. ........ */
                    /* la.decode:346 */
                    return LISA_FCVTRP_H_S;
                case 0x12:
                    /* 00000001 00011000 010010.. ........ */
                    /* la.decode:347 */
                    return LISA_FCVTRP_H_D;
                case 0x13:
                    /* 00000001 00011000 010011.. ........ */
                    /* la.decode:348 */
                    return LISA_FCVTRP_H_Q;
                case 0x14:
                    /* 00000001 00011000 010100.. ........ */
                    /* la.decode:349 */
                    return LISA_FCVTRP_S_H;
                case 0x16:
                    /* 00000001 00011000 010110.. ........ */
                    /* la.decode:350 */
                    return LISA_FCVTRP_S_D;
                case 0x17:
                    /* 00000001 00011000 010111.. ........ */
                    /* la.decode:351 */
                    return LISA_FCVTRP_S_Q;
                case 0x18:
                    /* 00000001 00011000 011000.. ........ */
                    /* la.decode:352 */
                    return LISA_FCVTRP_D_H;
                case 0x19:
                    /* 00000001 00011000 011001.. ........ */
                    /* la.decode:353 */
                    return LISA_FCVTRP_D_S;
                case 0x1b:
                    /* 00000001 00011000 011011.. ........ */
                    /* la.decode:354 */
                    return LISA_FCVTRP_D_Q;
                case 0x1c:
                    /* 00000001 00011000 011100.. ........ */
                    /* la.decode:355 */
                    return LISA_FCVTRP_Q_H;
                case 0x1d:
                    /* 00000001 00011000 011101.. ........ */
                    /* la.decode:356 */
                    return LISA_FCVTRP_Q_S;
                case 0x1e:
                    /* 00000001 00011000 011110.. ........ */
                    /* la.decode:357 */
                    return LISA_FCVTRP_Q_D;
                }
                return LISA_INVALID;
            case 0x31:
                /* 00000001 00011000 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x1:
                    /* 00000001 00011000 100001.. ........ */
                    /* la.decode:358 */
                    return LISA_FCVTRZ_H_S;
                case 0x2:
                    /* 00000001 00011000 100010.. ........ */
                    /* la.decode:359 */
                    return LISA_FCVTRZ_H_D;
                case 0x3:
                    /* 00000001 00011000 100011.. ........ */
                    /* la.decode:360 */
                    return LISA_FCVTRZ_H_Q;
                case 0x4:
                    /* 00000001 00011000 100100.. ........ */
                    /* la.decode:361 */
                    return LISA_FCVTRZ_S_H;
                case 0x6:
                    /* 00000001 00011000 100110.. ........ */
                    /* la.decode:362 */
                    return LISA_FCVTRZ_S_D;
                case 0x7:
                    /* 00000001 00011000 100111.. ........ */
                    /* la.decode:363 */
                    return LISA_FCVTRZ_S_Q;
                case 0x8:
                    /* 00000001 00011000 101000.. ........ */
                    /* la.decode:364 */
                    return LISA_FCVTRZ_D_H;
                case 0x9:
                    /* 00000001 00011000 101001.. ........ */
                    /* la.decode:365 */
                    return LISA_FCVTRZ_D_S;
                case 0xb:
                    /* 00000001 00011000 101011.. ........ */
                    /* la.decode:366 */
                    return LISA_FCVTRZ_D_Q;
                case 0xc:
                    /* 00000001 00011000 101100.. ........ */
                    /* la.decode:367 */
                    return LISA_FCVTRZ_Q_H;
                case 0xd:
                    /* 00000001 00011000 101101.. ........ */
                    /* la.decode:368 */
                    return LISA_FCVTRZ_Q_S;
                case 0xe:
                    /* 00000001 00011000 101110.. ........ */
                    /* la.decode:369 */
                    return LISA_FCVTRZ_Q_D;
                case 0x11:
                    /* 00000001 00011000 110001.. ........ */
                    /* la.decode:370 */
                    return LISA_FCVTRNE_H_S;
                case 0x12:
                    /* 00000001 00011000 110010.. ........ */
                    /* la.decode:371 */
                    return LISA_FCVTRNE_H_D;
                case 0x13:
                    /* 00000001 00011000 110011.. ........ */
                    /* la.decode:372 */
                    return LISA_FCVTRNE_H_Q;
                case 0x14:
                    /* 00000001 00011000 110100.. ........ */
                    /* la.decode:373 */
                    return LISA_FCVTRNE_S_H;
                case 0x16:
                    /* 00000001 00011000 110110.. ........ */
                    /* la.decode:374 */
                    return LISA_FCVTRNE_S_D;
                case 0x17:
                    /* 00000001 00011000 110111.. ........ */
                    /* la.decode:375 */
                    return LISA_FCVTRNE_S_Q;
                case 0x18:
                    /* 00000001 00011000 111000.. ........ */
                    /* la.decode:376 */
                    return LISA_FCVTRNE_D_H;
                case 0x19:
                    /* 00000001 00011000 111001.. ........ */
                    /* la.decode:377 */
                    return LISA_FCVTRNE_D_S;
                case 0x1b:
                    /* 00000001 00011000 111011.. ........ */
                    /* la.decode:378 */
                    return LISA_FCVTRNE_D_Q;
                case 0x1c:
                    /* 00000001 00011000 111100.. ........ */
                    /* la.decode:379 */
                    return LISA_FCVTRNE_Q_H;
                case 0x1d:
                    /* 00000001 00011000 111101.. ........ */
                    /* la.decode:380 */
                    return LISA_FCVTRNE_Q_S;
                case 0x1e:
                    /* 00000001 00011000 111110.. ........ */
                    /* la.decode:381 */
                    return LISA_FCVTRNE_Q_D;
                }
                return LISA_INVALID;
            case 0x32:
                /* 00000001 00011001 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x1:
                    /* 00000001 00011001 000001.. ........ */
                    /* la.decode:382 */
                    return LISA_FCVT_H_S;
                case 0x2:
                    /* 00000001 00011001 000010.. ........ */
                    /* la.decode:383 */
                    return LISA_FCVT_H_D;
                case 0x3:
                    /* 00000001 00011001 000011.. ........ */
                    /* la.decode:384 */
                    return LISA_FCVT_H_Q;
                case 0x4:
                    /* 00000001 00011001 000100.. ........ */
                    /* la.decode:385 */
                    return LISA_FCVT_S_H;
                case 0x6:
                    /* 00000001 00011001 000110.. ........ */
                    /* la.decode:386 */
                    return LISA_FCVT_S_D;
                case 0x7:
                    /* 00000001 00011001 000111.. ........ */
                    /* la.decode:387 */
                    return LISA_FCVT_S_Q;
                case 0x8:
                    /* 00000001 00011001 001000.. ........ */
                    /* la.decode:388 */
                    return LISA_FCVT_D_H;
                case 0x9:
                    /* 00000001 00011001 001001.. ........ */
                    /* la.decode:389 */
                    return LISA_FCVT_D_S;
                case 0xb:
                    /* 00000001 00011001 001011.. ........ */
                    /* la.decode:390 */
                    return LISA_FCVT_D_Q;
                case 0xc:
                    /* 00000001 00011001 001100.. ........ */
                    /* la.decode:391 */
                    return LISA_FCVT_Q_H;
                case 0xd:
                    /* 00000001 00011001 001101.. ........ */
                    /* la.decode:392 */
                    return LISA_FCVT_Q_S;
                case 0xe:
                    /* 00000001 00011001 001110.. ........ */
                    /* la.decode:393 */
                    return LISA_FCVT_Q_D;
                }
                return LISA_INVALID;
            case 0x34:
                /* 00000001 00011010 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011010 000000.. ........ */
                    /* la.decode:394 */
                    return LISA_FTINTRM_W_H;
                case 0x1:
                    /* 00000001 00011010 000001.. ........ */
                    /* la.decode:395 */
                    return LISA_FTINTRM_W_S;
                case 0x2:
                    /* 00000001 00011010 000010.. ........ */
                    /* la.decode:396 */
                    return LISA_FTINTRM_W_D;
                case 0x3:
                    /* 00000001 00011010 000011.. ........ */
                    /* la.decode:397 */
                    return LISA_FTINTRM_W_Q;
                case 0x4:
                    /* 00000001 00011010 000100.. ........ */
                    /* la.decode:398 */
                    return LISA_FTINTRM_WU_H;
                case 0x5:
                    /* 00000001 00011010 000101.. ........ */
                    /* la.decode:399 */
                    return LISA_FTINTRM_WU_S;
                case 0x6:
                    /* 00000001 00011010 000110.. ........ */
                    /* la.decode:400 */
                    return LISA_FTINTRM_WU_D;
                case 0x7:
                    /* 00000001 00011010 000111.. ........ */
                    /* la.decode:401 */
                    return LISA_FTINTRM_WU_Q;
                case 0x8:
                    /* 00000001 00011010 001000.. ........ */
                    /* la.decode:402 */
                    return LISA_FTINTRM_L_H;
                case 0x9:
                    /* 00000001 00011010 001001.. ........ */
                    /* la.decode:403 */
                    return LISA_FTINTRM_L_S;
                case 0xa:
                    /* 00000001 00011010 001010.. ........ */
                    /* la.decode:404 */
                    return LISA_FTINTRM_L_D;
                case 0xb:
                    /* 00000001 00011010 001011.. ........ */
                    /* la.decode:405 */
                    return LISA_FTINTRM_L_Q;
                case 0xc:
                    /* 00000001 00011010 001100.. ........ */
                    /* la.decode:406 */
                    return LISA_FTINTRM_LU_H;
                case 0xd:
                    /* 00000001 00011010 001101.. ........ */
                    /* la.decode:407 */
                    return LISA_FTINTRM_LU_S;
                case 0xe:
                    /* 00000001 00011010 001110.. ........ */
                    /* la.decode:408 */
                    return LISA_FTINTRM_LU_D;
                case 0xf:
                    /* 00000001 00011010 001111.. ........ */
                    /* la.decode:409 */
                    return LISA_FTINTRM_LU_Q;
                case 0x10:
                    /* 00000001 00011010 010000.. ........ */
                    /* la.decode:410 */
                    return LISA_FTINTRP_W_H;
                case 0x11:
                    /* 00000001 00011010 010001.. ........ */
                    /* la.decode:411 */
                    return LISA_FTINTRP_W_S;
                case 0x12:
                    /* 00000001 00011010 010010.. ........ */
                    /* la.decode:412 */
                    return LISA_FTINTRP_W_D;
                case 0x13:
                    /* 00000001 00011010 010011.. ........ */
                    /* la.decode:413 */
                    return LISA_FTINTRP_W_Q;
                case 0x14:
                    /* 00000001 00011010 010100.. ........ */
                    /* la.decode:414 */
                    return LISA_FTINTRP_WU_H;
                case 0x15:
                    /* 00000001 00011010 010101.. ........ */
                    /* la.decode:415 */
                    return LISA_FTINTRP_WU_S;
                case 0x16:
                    /* 00000001 00011010 010110.. ........ */
                    /* la.decode:416 */
                    return LISA_FTINTRP_WU_D;
                case 0x17:
                    /* 00000001 00011010 010111.. ........ */
                    /* la.decode:417 */
                    return LISA_FTINTRP_WU_Q;
                case 0x18:
                    /* 00000001 00011010 011000.. ........ */
                    /* la.decode:418 */
                    return LISA_FTINTRP_L_H;
                case 0x19:
                    /* 00000001 00011010 011001.. ........ */
                    /* la.decode:419 */
                    return LISA_FTINTRP_L_S;
                case 0x1a:
                    /* 00000001 00011010 011010.. ........ */
                    /* la.decode:420 */
                    return LISA_FTINTRP_L_D;
                case 0x1b:
                    /* 00000001 00011010 011011.. ........ */
                    /* la.decode:421 */
                    return LISA_FTINTRP_L_Q;
                case 0x1c:
                    /* 00000001 00011010 011100.. ........ */
                    /* la.decode:422 */
                    return LISA_FTINTRP_LU_H;
                case 0x1d:
                    /* 00000001 00011010 011101.. ........ */
                    /* la.decode:423 */
                    return LISA_FTINTRP_LU_S;
                case 0x1e:
                    /* 00000001 00011010 011110.. ........ */
                    /* la.decode:424 */
                    return LISA_FTINTRP_LU_D;
                case 0x1f:
                    /* 00000001 00011010 011111.. ........ */
                    /* la.decode:425 */
                    return LISA_FTINTRP_LU_Q;
                }
                return LISA_INVALID;
            case 0x35:
                /* 00000001 00011010 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011010 100000.. ........ */
                    /* la.decode:426 */
                    return LISA_FTINTRZ_W_H;
                case 0x1:
                    /* 00000001 00011010 100001.. ........ */
                    /* la.decode:427 */
                    return LISA_FTINTRZ_W_S;
                case 0x2:
                    /* 00000001 00011010 100010.. ........ */
                    /* la.decode:428 */
                    return LISA_FTINTRZ_W_D;
                case 0x3:
                    /* 00000001 00011010 100011.. ........ */
                    /* la.decode:429 */
                    return LISA_FTINTRZ_W_Q;
                case 0x4:
                    /* 00000001 00011010 100100.. ........ */
                    /* la.decode:430 */
                    return LISA_FTINTRZ_WU_H;
                case 0x5:
                    /* 00000001 00011010 100101.. ........ */
                    /* la.decode:431 */
                    return LISA_FTINTRZ_WU_S;
                case 0x6:
                    /* 00000001 00011010 100110.. ........ */
                    /* la.decode:432 */
                    return LISA_FTINTRZ_WU_D;
                case 0x7:
                    /* 00000001 00011010 100111.. ........ */
                    /* la.decode:433 */
                    return LISA_FTINTRZ_WU_Q;
                case 0x8:
                    /* 00000001 00011010 101000.. ........ */
                    /* la.decode:434 */
                    return LISA_FTINTRZ_L_H;
                case 0x9:
                    /* 00000001 00011010 101001.. ........ */
                    /* la.decode:435 */
                    return LISA_FTINTRZ_L_S;
                case 0xa:
                    /* 00000001 00011010 101010.. ........ */
                    /* la.decode:436 */
                    return LISA_FTINTRZ_L_D;
                case 0xb:
                    /* 00000001 00011010 101011.. ........ */
                    /* la.decode:437 */
                    return LISA_FTINTRZ_L_Q;
                case 0xc:
                    /* 00000001 00011010 101100.. ........ */
                    /* la.decode:438 */
                    return LISA_FTINTRZ_LU_H;
                case 0xd:
                    /* 00000001 00011010 101101.. ........ */
                    /* la.decode:439 */
                    return LISA_FTINTRZ_LU_S;
                case 0xe:
                    /* 00000001 00011010 101110.. ........ */
                    /* la.decode:440 */
                    return LISA_FTINTRZ_LU_D;
                case 0xf:
                    /* 00000001 00011010 101111.. ........ */
                    /* la.decode:441 */
                    return LISA_FTINTRZ_LU_Q;
                case 0x10:
                    /* 00000001 00011010 110000.. ........ */
                    /* la.decode:442 */
                    return LISA_FTINTRNE_W_H;
                case 0x11:
                    /* 00000001 00011010 110001.. ........ */
                    /* la.decode:443 */
                    return LISA_FTINTRNE_W_S;
                case 0x12:
                    /* 00000001 00011010 110010.. ........ */
                    /* la.decode:444 */
                    return LISA_FTINTRNE_W_D;
                case 0x13:
                    /* 00000001 00011010 110011.. ........ */
                    /* la.decode:445 */
                    return LISA_FTINTRNE_W_Q;
                case 0x14:
                    /* 00000001 00011010 110100.. ........ */
                    /* la.decode:446 */
                    return LISA_FTINTRNE_WU_H;
                case 0x15:
                    /* 00000001 00011010 110101.. ........ */
                    /* la.decode:447 */
                    return LISA_FTINTRNE_WU_S;
                case 0x16:
                    /* 00000001 00011010 110110.. ........ */
                    /* la.decode:448 */
                    return LISA_FTINTRNE_WU_D;
                case 0x17:
                    /* 00000001 00011010 110111.. ........ */
                    /* la.decode:449 */
                    return LISA_FTINTRNE_WU_Q;
                case 0x18:
                    /* 00000001 00011010 111000.. ........ */
                    /* la.decode:450 */
                    return LISA_FTINTRNE_L_H;
                case 0x19:
                    /* 00000001 00011010 111001.. ........ */
                    /* la.decode:451 */
                    return LISA_FTINTRNE_L_S;
                case 0x1a:
                    /* 00000001 00011010 111010.. ........ */
                    /* la.decode:452 */
                    return LISA_FTINTRNE_L_D;
                case 0x1b:
                    /* 00000001 00011010 111011.. ........ */
                    /* la.decode:453 */
                    return LISA_FTINTRNE_L_Q;
                case 0x1c:
                    /* 00000001 00011010 111100.. ........ */
                    /* la.decode:454 */
                    return LISA_FTINTRNE_LU_H;
                case 0x1d:
                    /* 00000001 00011010 111101.. ........ */
                    /* la.decode:455 */
                    return LISA_FTINTRNE_LU_S;
                case 0x1e:
                    /* 00000001 00011010 111110.. ........ */
                    /* la.decode:456 */
                    return LISA_FTINTRNE_LU_D;
                case 0x1f:
                    /* 00000001 00011010 111111.. ........ */
                    /* la.decode:457 */
                    return LISA_FTINTRNE_LU_Q;
                }
                return LISA_INVALID;
            case 0x36:
                /* 00000001 00011011 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011011 000000.. ........ */
                    /* la.decode:458 */
                    return LISA_FTINT_W_H;
                case 0x1:
                    /* 00000001 00011011 000001.. ........ */
                    /* la.decode:459 */
                    return LISA_FTINT_W_S;
                case 0x2:
                    /* 00000001 00011011 000010.. ........ */
                    /* la.decode:460 */
                    return LISA_FTINT_W_D;
                case 0x3:
                    /* 00000001 00011011 000011.. ........ */
                    /* la.decode:461 */
                    return LISA_FTINT_W_Q;
                case 0x4:
                    /* 00000001 00011011 000100.. ........ */
                    /* la.decode:462 */
                    return LISA_FTINT_WU_H;
                case 0x5:
                    /* 00000001 00011011 000101.. ........ */
                    /* la.decode:463 */
                    return LISA_FTINT_WU_S;
                case 0x6:
                    /* 00000001 00011011 000110.. ........ */
                    /* la.decode:464 */
                    return LISA_FTINT_WU_D;
                case 0x7:
                    /* 00000001 00011011 000111.. ........ */
                    /* la.decode:465 */
                    return LISA_FTINT_WU_Q;
                case 0x8:
                    /* 00000001 00011011 001000.. ........ */
                    /* la.decode:466 */
                    return LISA_FTINT_L_H;
                case 0x9:
                    /* 00000001 00011011 001001.. ........ */
                    /* la.decode:467 */
                    return LISA_FTINT_L_S;
                case 0xa:
                    /* 00000001 00011011 001010.. ........ */
                    /* la.decode:468 */
                    return LISA_FTINT_L_D;
                case 0xb:
                    /* 00000001 00011011 001011.. ........ */
                    /* la.decode:469 */
                    return LISA_FTINT_L_Q;
                case 0xc:
                    /* 00000001 00011011 001100.. ........ */
                    /* la.decode:470 */
                    return LISA_FTINT_LU_H;
                case 0xd:
                    /* 00000001 00011011 001101.. ........ */
                    /* la.decode:471 */
                    return LISA_FTINT_LU_S;
                case 0xe:
                    /* 00000001 00011011 001110.. ........ */
                    /* la.decode:472 */
                    return LISA_FTINT_LU_D;
                case 0xf:
                    /* 00000001 00011011 001111.. ........ */
                    /* la.decode:473 */
                    return LISA_FTINT_LU_Q;
                }
                return LISA_INVALID;
            case 0x38:
                /* 00000001 00011100 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011100 000000.. ........ */
                    /* la.decode:474 */
                    return LISA_FFINTRM_H_W;
                case 0x1:
                    /* 00000001 00011100 000001.. ........ */
                    /* la.decode:475 */
                    return LISA_FFINTRM_H_WU;
                case 0x2:
                    /* 00000001 00011100 000010.. ........ */
                    /* la.decode:476 */
                    return LISA_FFINTRM_H_L;
                case 0x3:
                    /* 00000001 00011100 000011.. ........ */
                    /* la.decode:477 */
                    return LISA_FFINTRM_H_LU;
                case 0x4:
                    /* 00000001 00011100 000100.. ........ */
                    /* la.decode:478 */
                    return LISA_FFINTRM_S_W;
                case 0x5:
                    /* 00000001 00011100 000101.. ........ */
                    /* la.decode:479 */
                    return LISA_FFINTRM_S_WU;
                case 0x6:
                    /* 00000001 00011100 000110.. ........ */
                    /* la.decode:480 */
                    return LISA_FFINTRM_S_L;
                case 0x7:
                    /* 00000001 00011100 000111.. ........ */
                    /* la.decode:481 */
                    return LISA_FFINTRM_S_LU;
                case 0x8:
                    /* 00000001 00011100 001000.. ........ */
                    /* la.decode:482 */
                    return LISA_FFINTRM_D_W;
                case 0x9:
                    /* 00000001 00011100 001001.. ........ */
                    /* la.decode:483 */
                    return LISA_FFINTRM_D_WU;
                case 0xa:
                    /* 00000001 00011100 001010.. ........ */
                    /* la.decode:484 */
                    return LISA_FFINTRM_D_L;
                case 0xb:
                    /* 00000001 00011100 001011.. ........ */
                    /* la.decode:485 */
                    return LISA_FFINTRM_D_LU;
                case 0xc:
                    /* 00000001 00011100 001100.. ........ */
                    /* la.decode:486 */
                    return LISA_FFINTRM_Q_W;
                case 0xd:
                    /* 00000001 00011100 001101.. ........ */
                    /* la.decode:487 */
                    return LISA_FFINTRM_Q_WU;
                case 0xe:
                    /* 00000001 00011100 001110.. ........ */
                    /* la.decode:488 */
                    return LISA_FFINTRM_Q_L;
                case 0xf:
                    /* 00000001 00011100 001111.. ........ */
                    /* la.decode:489 */
                    return LISA_FFINTRM_Q_LU;
                case 0x10:
                    /* 00000001 00011100 010000.. ........ */
                    /* la.decode:490 */
                    return LISA_FFINTRP_H_W;
                case 0x11:
                    /* 00000001 00011100 010001.. ........ */
                    /* la.decode:491 */
                    return LISA_FFINTRP_H_WU;
                case 0x12:
                    /* 00000001 00011100 010010.. ........ */
                    /* la.decode:492 */
                    return LISA_FFINTRP_H_L;
                case 0x13:
                    /* 00000001 00011100 010011.. ........ */
                    /* la.decode:493 */
                    return LISA_FFINTRP_H_LU;
                case 0x14:
                    /* 00000001 00011100 010100.. ........ */
                    /* la.decode:494 */
                    return LISA_FFINTRP_S_W;
                case 0x15:
                    /* 00000001 00011100 010101.. ........ */
                    /* la.decode:495 */
                    return LISA_FFINTRP_S_WU;
                case 0x16:
                    /* 00000001 00011100 010110.. ........ */
                    /* la.decode:496 */
                    return LISA_FFINTRP_S_L;
                case 0x17:
                    /* 00000001 00011100 010111.. ........ */
                    /* la.decode:497 */
                    return LISA_FFINTRP_S_LU;
                case 0x18:
                    /* 00000001 00011100 011000.. ........ */
                    /* la.decode:498 */
                    return LISA_FFINTRP_D_W;
                case 0x19:
                    /* 00000001 00011100 011001.. ........ */
                    /* la.decode:499 */
                    return LISA_FFINTRP_D_WU;
                case 0x1a:
                    /* 00000001 00011100 011010.. ........ */
                    /* la.decode:500 */
                    return LISA_FFINTRP_D_L;
                case 0x1b:
                    /* 00000001 00011100 011011.. ........ */
                    /* la.decode:501 */
                    return LISA_FFINTRP_D_LU;
                case 0x1c:
                    /* 00000001 00011100 011100.. ........ */
                    /* la.decode:502 */
                    return LISA_FFINTRP_Q_W;
                case 0x1d:
                    /* 00000001 00011100 011101.. ........ */
                    /* la.decode:503 */
                    return LISA_FFINTRP_Q_WU;
                case 0x1e:
                    /* 00000001 00011100 011110.. ........ */
                    /* la.decode:504 */
                    return LISA_FFINTRP_Q_L;
                case 0x1f:
                    /* 00000001 00011100 011111.. ........ */
                    /* la.decode:505 */
                    return LISA_FFINTRP_Q_LU;
                }
                return LISA_INVALID;
            case 0x39:
                /* 00000001 00011100 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011100 100000.. ........ */
                    /* la.decode:506 */
                    return LISA_FFINTRZ_H_W;
                case 0x1:
                    /* 00000001 00011100 100001.. ........ */
                    /* la.decode:507 */
                    return LISA_FFINTRZ_H_WU;
                case 0x2:
                    /* 00000001 00011100 100010.. ........ */
                    /* la.decode:508 */
                    return LISA_FFINTRZ_H_L;
                case 0x3:
                    /* 00000001 00011100 100011.. ........ */
                    /* la.decode:509 */
                    return LISA_FFINTRZ_H_LU;
                case 0x4:
                    /* 00000001 00011100 100100.. ........ */
                    /* la.decode:510 */
                    return LISA_FFINTRZ_S_W;
                case 0x5:
                    /* 00000001 00011100 100101.. ........ */
                    /* la.decode:511 */
                    return LISA_FFINTRZ_S_WU;
                case 0x6:
                    /* 00000001 00011100 100110.. ........ */
                    /* la.decode:512 */
                    return LISA_FFINTRZ_S_L;
                case 0x7:
                    /* 00000001 00011100 100111.. ........ */
                    /* la.decode:513 */
                    return LISA_FFINTRZ_S_LU;
                case 0x8:
                    /* 00000001 00011100 101000.. ........ */
                    /* la.decode:514 */
                    return LISA_FFINTRZ_D_W;
                case 0x9:
                    /* 00000001 00011100 101001.. ........ */
                    /* la.decode:515 */
                    return LISA_FFINTRZ_D_WU;
                case 0xa:
                    /* 00000001 00011100 101010.. ........ */
                    /* la.decode:516 */
                    return LISA_FFINTRZ_D_L;
                case 0xb:
                    /* 00000001 00011100 101011.. ........ */
                    /* la.decode:517 */
                    return LISA_FFINTRZ_D_LU;
                case 0xc:
                    /* 00000001 00011100 101100.. ........ */
                    /* la.decode:518 */
                    return LISA_FFINTRZ_Q_W;
                case 0xd:
                    /* 00000001 00011100 101101.. ........ */
                    /* la.decode:519 */
                    return LISA_FFINTRZ_Q_WU;
                case 0xe:
                    /* 00000001 00011100 101110.. ........ */
                    /* la.decode:520 */
                    return LISA_FFINTRZ_Q_L;
                case 0xf:
                    /* 00000001 00011100 101111.. ........ */
                    /* la.decode:521 */
                    return LISA_FFINTRZ_Q_LU;
                case 0x10:
                    /* 00000001 00011100 110000.. ........ */
                    /* la.decode:522 */
                    return LISA_FFINTRNE_H_W;
                case 0x11:
                    /* 00000001 00011100 110001.. ........ */
                    /* la.decode:523 */
                    return LISA_FFINTRNE_H_WU;
                case 0x12:
                    /* 00000001 00011100 110010.. ........ */
                    /* la.decode:524 */
                    return LISA_FFINTRNE_H_L;
                case 0x13:
                    /* 00000001 00011100 110011.. ........ */
                    /* la.decode:525 */
                    return LISA_FFINTRNE_H_LU;
                case 0x14:
                    /* 00000001 00011100 110100.. ........ */
                    /* la.decode:526 */
                    return LISA_FFINTRNE_S_W;
                case 0x15:
                    /* 00000001 00011100 110101.. ........ */
                    /* la.decode:527 */
                    return LISA_FFINTRNE_S_WU;
                case 0x16:
                    /* 00000001 00011100 110110.. ........ */
                    /* la.decode:528 */
                    return LISA_FFINTRNE_S_L;
                case 0x17:
                    /* 00000001 00011100 110111.. ........ */
                    /* la.decode:529 */
                    return LISA_FFINTRNE_S_LU;
                case 0x18:
                    /* 00000001 00011100 111000.. ........ */
                    /* la.decode:530 */
                    return LISA_FFINTRNE_D_W;
                case 0x19:
                    /* 00000001 00011100 111001.. ........ */
                    /* la.decode:531 */
                    return LISA_FFINTRNE_D_WU;
                case 0x1a:
                    /* 00000001 00011100 111010.. ........ */
                    /* la.decode:532 */
                    return LISA_FFINTRNE_D_L;
                case 0x1b:
                    /* 00000001 00011100 111011.. ........ */
                    /* la.decode:533 */
                    return LISA_FFINTRNE_D_LU;
                case 0x1c:
                    /* 00000001 00011100 111100.. ........ */
                    /* la.decode:534 */
                    return LISA_FFINTRNE_Q_W;
                case 0x1d:
                    /* 00000001 00011100 111101.. ........ */
                    /* la.decode:535 */
                    return LISA_FFINTRNE_Q_WU;
                case 0x1e:
                    /* 00000001 00011100 111110.. ........ */
                    /* la.decode:536 */
                    return LISA_FFINTRNE_Q_L;
                case 0x1f:
                    /* 00000001 00011100 111111.. ........ */
                    /* la.decode:537 */
                    return LISA_FFINTRNE_Q_LU;
                }
                return LISA_INVALID;
            case 0x3a:
                /* 00000001 00011101 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011101 000000.. ........ */
                    /* la.decode:538 */
                    return LISA_FFINT_H_W;
                case 0x1:
                    /* 00000001 00011101 000001.. ........ */
                    /* la.decode:539 */
                    return LISA_FFINT_H_WU;
                case 0x2:
                    /* 00000001 00011101 000010.. ........ */
                    /* la.decode:540 */
                    return LISA_FFINT_H_L;
                case 0x3:
                    /* 00000001 00011101 000011.. ........ */
                    /* la.decode:541 */
                    return LISA_FFINT_H_LU;
                case 0x4:
                    /* 00000001 00011101 000100.. ........ */
                    /* la.decode:542 */
                    return LISA_FFINT_S_W;
                case 0x5:
                    /* 00000001 00011101 000101.. ........ */
                    /* la.decode:543 */
                    return LISA_FFINT_S_WU;
                case 0x6:
                    /* 00000001 00011101 000110.. ........ */
                    /* la.decode:544 */
                    return LISA_FFINT_S_L;
                case 0x7:
                    /* 00000001 00011101 000111.. ........ */
                    /* la.decode:545 */
                    return LISA_FFINT_S_LU;
                case 0x8:
                    /* 00000001 00011101 001000.. ........ */
                    /* la.decode:546 */
                    return LISA_FFINT_D_W;
                case 0x9:
                    /* 00000001 00011101 001001.. ........ */
                    /* la.decode:547 */
                    return LISA_FFINT_D_WU;
                case 0xa:
                    /* 00000001 00011101 001010.. ........ */
                    /* la.decode:548 */
                    return LISA_FFINT_D_L;
                case 0xb:
                    /* 00000001 00011101 001011.. ........ */
                    /* la.decode:549 */
                    return LISA_FFINT_D_LU;
                case 0xc:
                    /* 00000001 00011101 001100.. ........ */
                    /* la.decode:550 */
                    return LISA_FFINT_Q_W;
                case 0xd:
                    /* 00000001 00011101 001101.. ........ */
                    /* la.decode:551 */
                    return LISA_FFINT_Q_WU;
                case 0xe:
                    /* 00000001 00011101 001110.. ........ */
                    /* la.decode:552 */
                    return LISA_FFINT_Q_L;
                case 0xf:
                    /* 00000001 00011101 001111.. ........ */
                    /* la.decode:553 */
                    return LISA_FFINT_Q_LU;
                }
                return LISA_INVALID;
            case 0x3c:
                /* 00000001 00011110 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 00000001 00011110 000000.. ........ */
                    /* la.decode:554 */
                    return LISA_FRINTRM_H;
                case 0x1:
                    /* 00000001 00011110 000001.. ........ */
                    /* la.decode:555 */
                    return LISA_FRINTRM_S;
                case 0x2:
                    /* 00000001 00011110 000010.. ........ */
                    /* la.decode:556 */
                    return LISA_FRINTRM_D;
                case 0x3:
                    /* 00000001 00011110 000011.. ........ */
                    /* la.decode:557 */
                    return LISA_FRINTRM_Q;
                case 0x4:
                    /* 00000001 00011110 000100.. ........ */
                    /* la.decode:558 */
                    return LISA_FRINTRP_H;
                case 0x5:
                    /* 00000001 00011110 000101.. ........ */
                    /* la.decode:559 */
                    return LISA_FRINTRP_S;
                case 0x6:
                    /* 00000001 00011110 000110.. ........ */
                    /* la.decode:560 */
                    return LISA_FRINTRP_D;
                case 0x7:
                    /* 00000001 00011110 000111.. ........ */
                    /* la.decode:561 */
                    return LISA_FRINTRP_Q;
                case 0x8:
                    /* 00000001 00011110 001000.. ........ */
                    /* la.decode:562 */
                    return LISA_FRINTRZ_H;
                case 0x9:
                    /* 00000001 00011110 001001.. ........ */
                    /* la.decode:563 */
                    return LISA_FRINTRZ_S;
                case 0xa:
                    /* 00000001 00011110 001010.. ........ */
                    /* la.decode:564 */
                    return LISA_FRINTRZ_D;
                case 0xb:
                    /* 00000001 00011110 001011.. ........ */
                    /* la.decode:565 */
                    return LISA_FRINTRZ_Q;
                case 0xc:
                    /* 00000001 00011110 001100.. ........ */
                    /* la.decode:566 */
                    return LISA_FRINTRNE_H;
                case 0xd:
                    /* 00000001 00011110 001101.. ........ */
                    /* la.decode:567 */
                    return LISA_FRINTRNE_S;
                case 0xe:
                    /* 00000001 00011110 001110.. ........ */
                    /* la.decode:568 */
                    return LISA_FRINTRNE_D;
                case 0xf:
                    /* 00000001 00011110 001111.. ........ */
                    /* la.decode:569 */
                    return LISA_FRINTRNE_Q;
                case 0x10:
                    /* 00000001 00011110 010000.. ........ */
                    /* la.decode:570 */
                    return LISA_FRINT_H;
                case 0x11:
                    /* 00000001 00011110 010001.. ........ */
                    /* la.decode:571 */
                    return LISA_FRINT_S;
                case 0x12:
                    /* 00000001 00011110 010010.. ........ */
                    /* la.decode:572 */
                    return LISA_FRINT_D;
                case 0x13:
                    /* 00000001 00011110 010011.. ........ */
                    /* la.decode:573 */
                    return LISA_FRINT_Q;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0x8:
            /* 00000010 00...... ........ ........ */
            /* la.decode:574 */
            return LISA_SLTI;
        case 0x9:
            /* 00000010 01...... ........ ........ */
            /* la.decode:575 */
            return LISA_SLTUI;
        case 0xa:
            /* 00000010 10...... ........ ........ */
            /* la.decode:576 */
            return LISA_ADDI_W;
        case 0xb:
            /* 00000010 11...... ........ ........ */
            /* la.decode:577 */
            return LISA_ADDI_D;
        case 0xc:
            /* 00000011 00...... ........ ........ */
            /* la.decode:578 */
            return LISA_LU52I_D;
        case 0xd:
            /* 00000011 01...... ........ ........ */
            /* la.decode:579 */
            return LISA_ANDI;
        case 0xe:
            /* 00000011 10...... ........ ........ */
            /* la.decode:580 */
            return LISA_ORI;
        case 0xf:
            /* 00000011 11...... ........ ........ */
            /* la.decode:581 */
            return LISA_XORI;
        }
        return LISA_INVALID;
    case 0x1:
        /* 000001.. ........ ........ ........ */
        switch ((insn >> 24) & 0x3) {
        case 0x0:
            /* 00000100 ........ ........ ........ */
            if ((insn & 0x000003e0) == 0x00000000) {
                /* 00000100 ........ ......00 000..... */
                /* la.decode:583 */
                return LISA_CSRRD;
            }
            if ((insn & 0x000003e0) == 0x00000020) {
                /* 00000100 ........ ......00 001..... */
                /* la.decode:584 */
                return LISA_CSRWR;
            }
            /* la.decode:585 */
            return LISA_CSRXCHG;
        case 0x1:
            /* 00000101 ........ ........ ........ */
            if ((insn & 0x000003e0) == 0x00000000) {
                /* 00000101 ........ ......00 000..... */
                /* la.decode:588 */
                return LISA_GCSRRD;
            }
            if ((insn & 0x000003e0) == 0x00000020) {
                /* 00000101 ........ ......00 001..... */
                /* la.decode:589 */
                return LISA_GCSRWR;
            }
            /* la.decode:590 */
            return LISA_GCSRXCHG;
        case 0x2:
            /* 00000110 ........ ........ ........ */
            switch ((insn >> 22) & 0x3) {
            case 0x0:
                /* 00000110 00...... ........ ........ */
                /* la.decode:592 */
                return LISA_CACHE;
            case 0x1:
                /* 00000110 01...... ........ ........ */
                switch ((insn >> 18) & 0xf) {
                case 0x0:
                    /* 00000110 010000.. ........ ........ */
                    /* la.decode:593 */
                    return LISA_LDDIR;
                case 0x1:
                    /* 00000110 010001.. ........ ........ */
                    switch (insn & 0x0000001f) {
                    case 0x00000000:
                        /* 00000110 010001.. ........ ...00000 */
                        /* la.decode:594 */
                        return LISA_LDPTE;
                    }
                    return LISA_INVALID;
                case 0x2:
                    /* 00000110 010010.. ........ ........ */
                    switch ((insn >> 15) & 0x7) {
                    case 0x0:
                        /* 00000110 01001000 0....... ........ */
                        switch ((insn >> 10) & 0x1f) {
                        case 0x0:
                            /* 00000110 01001000 000000.. ........ */
                            /* la.decode:595 */
                            return LISA_IOCSRRD_B;
                        case 0x1:
                            /* 00000110 01001000 000001.. ........ */
                            /* la.decode:596 */
                            return LISA_IOCSRRD_H;
                        case 0x2:
                            /* 00000110 01001000 000010.. ........ */
                            /* la.decode:597 */
                            return LISA_IOCSRRD_W;
                        case 0x3:
                            /* 00000110 01001000 000011.. ........ */
                            /* la.decode:598 */
                            return LISA_IOCSRRD_D;
                        case 0x4:
                            /* 00000110 01001000 000100.. ........ */
                            /* la.decode:599 */
                            return LISA_IOCSRWR_B;
                        case 0x5:
                            /* 00000110 01001000 000101.. ........ */
                            /* la.decode:600 */
                            return LISA_IOCSRWR_H;
                        case 0x6:
                            /* 00000110 01001000 000110.. ........ */
                            /* la.decode:601 */
                            return LISA_IOCSRWR_W;
                        case 0x7:
                            /* 00000110 01001000 000111.. ........ */
                            /* la.decode:602 */
                            return LISA_IOCSRWR_D;
                        case 0x8:
                            /* 00000110 01001000 001000.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00100000 00000000 */
                                /* la.decode:603 */
                                return LISA_TLBINV;
                            case 0x00000001:
                                /* 00000110 01001000 00100000 00000001 */
                                /* la.decode:604 */
                                return LISA_GTLBINV;
                            }
                            return LISA_INVALID;
                        case 0x9:
                            /* 00000110 01001000 001001.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00100100 00000000 */
                                /* la.decode:605 */
                                return LISA_TLBFLUSH;
                            case 0x00000001:
                                /* 00000110 01001000 00100100 00000001 */
                                /* la.decode:606 */
                                return LISA_GTLBFLUSH;
                            }
                            return LISA_INVALID;
                        case 0xa:
                            /* 00000110 01001000 001010.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00101000 00000000 */
                                /* la.decode:607 */
                                return LISA_TLBP;
                            case 0x00000001:
                                /* 00000110 01001000 00101000 00000001 */
                                /* la.decode:608 */
                                return LISA_GTLBP;
                            }
                            return LISA_INVALID;
                        case 0xb:
                            /* 00000110 01001000 001011.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00101100 00000000 */
                                /* la.decode:609 */
                                return LISA_TLBR;
                            case 0x00000001:
                                /* 00000110 01001000 00101100 00000001 */
                                /* la.decode:610 */
                                return LISA_GTLBR;
                            }
                            return LISA_INVALID;
                        case 0xc:
                            /* 00000110 01001000 001100.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00110000 00000000 */
                                /* la.decode:611 */
                                return LISA_TLBWI;
                            case 0x00000001:
                                /* 00000110 01001000 00110000 00000001 */
                                /* la.decode:612 */
                                return LISA_GTLBWI;
                            }
                            return LISA_INVALID;
                        case 0xd:
                            /* 00000110 01001000 001101.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00110100 00000000 */
                                /* la.decode:613 */
                                return LISA_TLBWR;
                            case 0x00000001:
                                /* 00000110 01001000 00110100 00000001 */
                                /* la.decode:614 */
                                return LISA_GTLBWR;
                            }
                            return LISA_INVALID;
                        case 0xe:
                            /* 00000110 01001000 001110.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00111000 00000000 */
                                /* la.decode:615 */
                                return LISA_ERET;
                            }
                            return LISA_INVALID;
                        case 0xf:
                            /* 00000110 01001000 001111.. ........ */
                            switch (insn & 0x000003ff) {
                            case 0x00000000:
                                /* 00000110 01001000 00111100 00000000 */
                                /* la.decode:616 */
                                return LISA_DERET;
                            }
                            return LISA_INVALID;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 00000110 01001000 1....... ........ */
                        /* la.decode:617 */
                        return LISA_WAIT;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        }
        return LISA_INVALID;
    case 0x3:
        /* 000011.. ........ ........ ........ */
        switch (insn & 0x03f00018) {
        case 0x00000000:
            /* 00001100 0000.... ........ ...00... */
            /* la.decode:618 */
            return LISA_FCMP_COND_H;
        case 0x00100000:
            /* 00001100 0001.... ........ ...00... */
            switch ((insn >> 15) & 0x1f) {
            case 0x0:
                /* 00001100 00010000 0....... ...00... */
                /* la.decode:641 */
                return LISA_FCMP_CAF_S;
            case 0x1:
                /* 00001100 00010000 1....... ...00... */
                /* la.decode:652 */
                return LISA_FCMP_SAF_S;
            case 0x2:
                /* 00001100 00010001 0....... ...00... */
                /* la.decode:645 */
                return LISA_FCMP_CLT_S;
            case 0x3:
                /* 00001100 00010001 1....... ...00... */
                /* la.decode:656 */
                return LISA_FCMP_SLT_S;
            case 0x4:
                /* 00001100 00010010 0....... ...00... */
                /* la.decode:643 */
                return LISA_FCMP_CEQ_S;
            case 0x5:
                /* 00001100 00010010 1....... ...00... */
                /* la.decode:654 */
                return LISA_FCMP_SEQ_S;
            case 0x6:
                /* 00001100 00010011 0....... ...00... */
                /* la.decode:647 */
                return LISA_FCMP_CLE_S;
            case 0x7:
                /* 00001100 00010011 1....... ...00... */
                /* la.decode:658 */
                return LISA_FCMP_SLE_S;
            case 0x8:
                /* 00001100 00010100 0....... ...00... */
                /* la.decode:642 */
                return LISA_FCMP_CUN_S;
            case 0x9:
                /* 00001100 00010100 1....... ...00... */
                /* la.decode:653 */
                return LISA_FCMP_SUN_S;
            case 0xa:
                /* 00001100 00010101 0....... ...00... */
                /* la.decode:646 */
                return LISA_FCMP_CULT_S;
            case 0xb:
                /* 00001100 00010101 1....... ...00... */
                /* la.decode:657 */
                return LISA_FCMP_SULT_S;
            case 0xc:
                /* 00001100 00010110 0....... ...00... */
                /* la.decode:644 */
                return LISA_FCMP_CUEQ_S;
            case 0xd:
                /* 00001100 00010110 1....... ...00... */
                /* la.decode:655 */
                return LISA_FCMP_SUEQ_S;
            case 0xe:
                /* 00001100 00010111 0....... ...00... */
                /* la.decode:648 */
                return LISA_FCMP_CULE_S;
            case 0xf:
                /* 00001100 00010111 1....... ...00... */
                /* la.decode:659 */
                return LISA_FCMP_SULE_S;
            case 0x10:
                /* 00001100 00011000 0....... ...00... */
                /* la.decode:649 */
                return LISA_FCMP_CNE_S;
            case 0x11:
                /* 00001100 00011000 1....... ...00... */
                /* la.decode:660 */
                return LISA_FCMP_SNE_S;
            case 0x14:
                /* 00001100 00011010 0....... ...00... */
                /* la.decode:650 */
                return LISA_FCMP_COR_S;
            case 0x15:
                /* 00001100 00011010 1....... ...00... */
                /* la.decode:661 */
                return LISA_FCMP_SOR_S;
            case 0x18:
                /* 00001100 00011100 0....... ...00... */
                /* la.decode:651 */
                return LISA_FCMP_CUNE_S;
            case 0x19:
                /* 00001100 00011100 1....... ...00... */
                /* la.decode:662 */
                return LISA_FCMP_SUNE_S;
            }
            return LISA_INVALID;
        case 0x00200000:
            /* 00001100 0010.... ........ ...00... */
            switch ((insn >> 15) & 0x1f) {
            case 0x0:
                /* 00001100 00100000 0....... ...00... */
                /* la.decode:619 */
                return LISA_FCMP_CAF_D;
            case 0x1:
                /* 00001100 00100000 1....... ...00... */
                /* la.decode:630 */
                return LISA_FCMP_SAF_D;
            case 0x2:
                /* 00001100 00100001 0....... ...00... */
                /* la.decode:623 */
                return LISA_FCMP_CLT_D;
            case 0x3:
                /* 00001100 00100001 1....... ...00... */
                /* la.decode:634 */
                return LISA_FCMP_SLT_D;
            case 0x4:
                /* 00001100 00100010 0....... ...00... */
                /* la.decode:621 */
                return LISA_FCMP_CEQ_D;
            case 0x5:
                /* 00001100 00100010 1....... ...00... */
                /* la.decode:632 */
                return LISA_FCMP_SEQ_D;
            case 0x6:
                /* 00001100 00100011 0....... ...00... */
                /* la.decode:625 */
                return LISA_FCMP_CLE_D;
            case 0x7:
                /* 00001100 00100011 1....... ...00... */
                /* la.decode:636 */
                return LISA_FCMP_SLE_D;
            case 0x8:
                /* 00001100 00100100 0....... ...00... */
                /* la.decode:620 */
                return LISA_FCMP_CUN_D;
            case 0x9:
                /* 00001100 00100100 1....... ...00... */
                /* la.decode:631 */
                return LISA_FCMP_SUN_D;
            case 0xa:
                /* 00001100 00100101 0....... ...00... */
                /* la.decode:624 */
                return LISA_FCMP_CULT_D;
            case 0xb:
                /* 00001100 00100101 1....... ...00... */
                /* la.decode:635 */
                return LISA_FCMP_SULT_D;
            case 0xc:
                /* 00001100 00100110 0....... ...00... */
                /* la.decode:622 */
                return LISA_FCMP_CUEQ_D;
            case 0xd:
                /* 00001100 00100110 1....... ...00... */
                /* la.decode:633 */
                return LISA_FCMP_SUEQ_D;
            case 0xe:
                /* 00001100 00100111 0....... ...00... */
                /* la.decode:626 */
                return LISA_FCMP_CULE_D;
            case 0xf:
                /* 00001100 00100111 1....... ...00... */
                /* la.decode:637 */
                return LISA_FCMP_SULE_D;
            case 0x10:
                /* 00001100 00101000 0....... ...00... */
                /* la.decode:627 */
                return LISA_FCMP_CNE_D;
            case 0x11:
                /* 00001100 00101000 1....... ...00... */
                /* la.decode:638 */
                return LISA_FCMP_SNE_D;
            case 0x14:
                /* 00001100 00101010 0....... ...00... */
                /* la.decode:628 */
                return LISA_FCMP_COR_D;
            case 0x15:
                /* 00001100 00101010 1....... ...00... */
                /* la.decode:639 */
                return LISA_FCMP_SOR_D;
            case 0x18:
                /* 00001100 00101100 0....... ...00... */
                /* la.decode:629 */
                return LISA_FCMP_CUNE_D;
            case 0x19:
                /* 00001100 00101100 1....... ...00... */
                /* la.decode:640 */
                return LISA_FCMP_SUNE_D;
            }
            return LISA_INVALID;
        }
        return LISA_INVALID;
    case 0x4:
        /* 000100.. ........ ........ ........ */
        /* la.decode:664 */
        return LISA_ADDU16I_D;
    case 0x5:
        /* 000101.. ........ ........ ........ */
        switch ((insn >> 25) & 0x1) {
        case 0x0:
            /* 0001010. ........ ........ ........ */
            /* la.decode:665 */
            return LISA_LU12I_W;
        case 0x1:
            /* 0001011. ........ ........ ........ */
            /* la.decode:666 */
            return LISA_LU32I_D;
        }
        return LISA_INVALID;
    case 0x6:
        /* 000110.. ........ ........ ........ */
        switch ((insn >> 25) & 0x1) {
        case 0x0:
            /* 0001100. ........ ........ ........ */
            /* la.decode:667 */
            return LISA_PCADDI;
        case 0x1:
            /* 0001101. ........ ........ ........ */
            /* la.decode:668 */
            return LISA_PCALAU12I;
        }
        return LISA_INVALID;
    case 0x7:
        /* 000111.. ........ ........ ........ */
        switch ((insn >> 25) & 0x1) {
        case 0x0:
            /* 0001110. ........ ........ ........ */
            /* la.decode:669 */
            return LISA_PCADDU12I;
        case 0x1:
            /* 0001111. ........ ........ ........ */
            /* la.decode:670 */
            return LISA_PCADDU18I;
        }
        return LISA_INVALID;
    case 0x8:
        /* 001000.. ........ ........ ........ */
        switch ((insn >> 24) & 0x3) {
        case 0x0:
            /* 00100000 ........ ........ ........ */
            /* la.decode:671 */
            return LISA_LL_W;
        case 0x1:
            /* 00100001 ........ ........ ........ */
            /* la.decode:672 */
            return LISA_SC_W;
        case 0x2:
            /* 00100010 ........ ........ ........ */
            /* la.decode:673 */
            return LISA_LL_D;
        case 0x3:
            /* 00100011 ........ ........ ........ */
            /* la.decode:674 */
            return LISA_SC_D;
        }
        return LISA_INVALID;
    case 0x9:
        /* 001001.. ........ ........ ........ */
        switch ((insn >> 24) & 0x3) {
        case 0x0:
            /* 00100100 ........ ........ ........ */
            /* la.decode:675 */
            return LISA_LDPTR_W;
        case 0x1:
            /* 00100101 ........ ........ ........ */
            /* la.decode:676 */
            return LISA_STPTR_W;
        case 0x2:
            /* 00100110 ........ ........ ........ */
            /* la.decode:677 */
            return LISA_LDPTR_D;
        case 0x3:
            /* 00100111 ........ ........ ........ */
            /* la.decode:678 */
            return LISA_STPTR_D;
        }
        return LISA_INVALID;
    case 0xa:
        /* 001010.. ........ ........ ........ */
        switch ((insn >> 22) & 0xf) {
        case 0x0:
            /* 00101000 00...... ........ ........ */
            /* la.decode:679 */
            return LISA_LD_B;
        case 0x1:
            /* 00101000 01...... ........ ........ */
            /* la.decode:680 */
            return LISA_LD_H;
        case 0x2:
            /* 00101000 10...... ........ ........ */
            /* la.decode:681 */
            return LISA_LD_W;
        case 0x3:
            /* 00101000 11...... ........ ........ */
            /* la.decode:682 */
            return LISA_LD_D;
        case 0x4:
            /* 00101001 00...... ........ ........ */
            /* la.decode:683 */
            return LISA_ST_B;
        case 0x5:
            /* 00101001 01...... ........ ........ */
            /* la.decode:684 */
            return LISA_ST_H;
        case 0x6:
            /* 00101001 10...... ........ ........ */
            /* la.decode:685 */
            return LISA_ST_W;
        case 0x7:
            /* 00101001 11...... ........ ........ */
            /* la.decode:686 */
            return LISA_ST_D;
        case 0x8:
            /* 00101010 00...... ........ ........ */
            /* la.decode:687 */
            return LISA_LD_BU;
        case 0x9:
            /* 00101010 01...... ........ ........ */
            /* la.decode:688 */
            return LISA_LD_HU;
        case 0xa:
            /* 00101010 10...... ........ ........ */
            /* la.decode:689 */
            return LISA_LD_WU;
        case 0xb:
            /* 00101010 11...... ........ ........ */
            /* la.decode:690 */
            return LISA_PRELD;
        case 0xc:
            /* 00101011 00...... ........ ........ */
            /* la.decode:691 */
            return LISA_FLD_S;
        case 0xd:
            /* 00101011 01...... ........ ........ */
            /* la.decode:692 */
            return LISA_FST_S;
        case 0xe:
            /* 00101011 10...... ........ ........ */
            /* la.decode:693 */
            return LISA_FLD_D;
        case 0xf:
            /* 00101011 11...... ........ ........ */
            /* la.decode:694 */
            return LISA_FST_D;
        }
        return LISA_INVALID;
    case 0xb:
        /* 001011.. ........ ........ ........ */
        switch ((insn >> 22) & 0xf) {
        case 0x0:
            /* 00101100 00...... ........ ........ */
            /* la.decode:695 */
            return LISA_VLD;
        case 0x1:
            /* 00101100 01...... ........ ........ */
            /* la.decode:696 */
            return LISA_VST;
        case 0x2:
            /* 00101100 10...... ........ ........ */
            /* la.decode:697 */
            return LISA_XVLD;
        case 0x3:
            /* 00101100 11...... ........ ........ */
            /* la.decode:698 */
            return LISA_XVST;
        case 0x8:
            /* 00101110 00...... ........ ........ */
            /* la.decode:699 */
            return LISA_LDL_W;
        case 0x9:
            /* 00101110 01...... ........ ........ */
            /* la.decode:700 */
            return LISA_LDR_W;
        case 0xa:
            /* 00101110 10...... ........ ........ */
            /* la.decode:701 */
            return LISA_LDL_D;
        case 0xb:
            /* 00101110 11...... ........ ........ */
            /* la.decode:702 */
            return LISA_LDR_D;
        case 0xc:
            /* 00101111 00...... ........ ........ */
            /* la.decode:703 */
            return LISA_STL_W;
        case 0xd:
            /* 00101111 01...... ........ ........ */
            /* la.decode:704 */
            return LISA_STR_W;
        case 0xe:
            /* 00101111 10...... ........ ........ */
            /* la.decode:705 */
            return LISA_STL_D;
        case 0xf:
            /* 00101111 11...... ........ ........ */
            /* la.decode:706 */
            return LISA_STR_D;
        }
        return LISA_INVALID;
    case 0xc:
        /* 001100.. ........ ........ ........ */
        switch ((insn >> 23) & 0x7) {
        case 0x0:
            /* 00110000 0....... ........ ........ */
            switch ((insn >> 21) & 0x3) {
            case 0x0:
                /* 00110000 000..... ........ ........ */
                switch ((insn >> 19) & 0x3) {
                case 0x2:
                    /* 00110000 00010... ........ ........ */
                    /* la.decode:707 */
                    return LISA_VLDREPL_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00110000 001..... ........ ........ */
                switch ((insn >> 20) & 0x1) {
                case 0x0:
                    /* 00110000 0010.... ........ ........ */
                    /* la.decode:708 */
                    return LISA_VLDREPL_W;
                }
                return LISA_INVALID;
            case 0x2:
                /* 00110000 010..... ........ ........ */
                /* la.decode:709 */
                return LISA_VLDREPL_H;
            }
            return LISA_INVALID;
        case 0x1:
            /* 00110000 1....... ........ ........ */
            switch ((insn >> 22) & 0x1) {
            case 0x0:
                /* 00110000 10...... ........ ........ */
                /* la.decode:710 */
                return LISA_VLDREPL_B;
            }
            return LISA_INVALID;
        case 0x2:
            /* 00110001 0....... ........ ........ */
            switch ((insn >> 21) & 0x3) {
            case 0x0:
                /* 00110001 000..... ........ ........ */
                switch ((insn >> 19) & 0x3) {
                case 0x2:
                    /* 00110001 00010... ........ ........ */
                    /* la.decode:711 */
                    return LISA_VSTELM_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00110001 001..... ........ ........ */
                switch ((insn >> 20) & 0x1) {
                case 0x0:
                    /* 00110001 0010.... ........ ........ */
                    /* la.decode:712 */
                    return LISA_VSTELM_W;
                }
                return LISA_INVALID;
            case 0x2:
                /* 00110001 010..... ........ ........ */
                /* la.decode:713 */
                return LISA_VSTELM_H;
            }
            return LISA_INVALID;
        case 0x3:
            /* 00110001 1....... ........ ........ */
            switch ((insn >> 22) & 0x1) {
            case 0x0:
                /* 00110001 10...... ........ ........ */
                /* la.decode:714 */
                return LISA_VSTELM_B;
            }
            return LISA_INVALID;
        case 0x4:
            /* 00110010 0....... ........ ........ */
            switch ((insn >> 21) & 0x3) {
            case 0x0:
                /* 00110010 000..... ........ ........ */
                switch ((insn >> 19) & 0x3) {
                case 0x2:
                    /* 00110010 00010... ........ ........ */
                    /* la.decode:715 */
                    return LISA_XVLDREPL_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00110010 001..... ........ ........ */
                switch ((insn >> 20) & 0x1) {
                case 0x0:
                    /* 00110010 0010.... ........ ........ */
                    /* la.decode:716 */
                    return LISA_XVLDREPL_W;
                }
                return LISA_INVALID;
            case 0x2:
                /* 00110010 010..... ........ ........ */
                /* la.decode:717 */
                return LISA_XVLDREPL_H;
            }
            return LISA_INVALID;
        case 0x5:
            /* 00110010 1....... ........ ........ */
            switch ((insn >> 22) & 0x1) {
            case 0x0:
                /* 00110010 10...... ........ ........ */
                /* la.decode:718 */
                return LISA_XVLDREPL_B;
            }
            return LISA_INVALID;
        case 0x6:
            /* 00110011 0....... ........ ........ */
            switch ((insn >> 22) & 0x1) {
            case 0x0:
                /* 00110011 00...... ........ ........ */
                switch ((insn >> 21) & 0x1) {
                case 0x0:
                    /* 00110011 000..... ........ ........ */
                    switch ((insn >> 20) & 0x1) {
                    case 0x1:
                        /* 00110011 0001.... ........ ........ */
                        /* la.decode:719 */
                        return LISA_XVSTELM_D;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 00110011 001..... ........ ........ */
                    /* la.decode:720 */
                    return LISA_XVSTELM_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 00110011 01...... ........ ........ */
                /* la.decode:721 */
                return LISA_XVSTELM_H;
            }
            return LISA_INVALID;
        case 0x7:
            /* 00110011 1....... ........ ........ */
            /* la.decode:722 */
            return LISA_XVSTELM_B;
        }
        return LISA_INVALID;
    case 0xe:
        /* 001110.. ........ ........ ........ */
        switch ((insn >> 15) & 0x7ff) {
        case 0x0:
            /* 00111000 00000000 0....... ........ */
            /* la.decode:723 */
            return LISA_LDX_B;
        case 0x8:
            /* 00111000 00000100 0....... ........ */
            /* la.decode:724 */
            return LISA_LDX_H;
        case 0x10:
            /* 00111000 00001000 0....... ........ */
            /* la.decode:725 */
            return LISA_LDX_W;
        case 0x18:
            /* 00111000 00001100 0....... ........ */
            /* la.decode:726 */
            return LISA_LDX_D;
        case 0x20:
            /* 00111000 00010000 0....... ........ */
            /* la.decode:727 */
            return LISA_STX_B;
        case 0x28:
            /* 00111000 00010100 0....... ........ */
            /* la.decode:728 */
            return LISA_STX_H;
        case 0x30:
            /* 00111000 00011000 0....... ........ */
            /* la.decode:729 */
            return LISA_STX_W;
        case 0x38:
            /* 00111000 00011100 0....... ........ */
            /* la.decode:730 */
            return LISA_STX_D;
        case 0x40:
            /* 00111000 00100000 0....... ........ */
            /* la.decode:731 */
            return LISA_LDX_BU;
        case 0x48:
            /* 00111000 00100100 0....... ........ */
            /* la.decode:732 */
            return LISA_LDX_HU;
        case 0x50:
            /* 00111000 00101000 0....... ........ */
            /* la.decode:733 */
            return LISA_LDX_WU;
        case 0x58:
            /* 00111000 00101100 0....... ........ */
            /* la.decode:734 */
            return LISA_PRELDX;
        case 0x60:
            /* 00111000 00110000 0....... ........ */
            /* la.decode:735 */
            return LISA_FLDX_S;
        case 0x68:
            /* 00111000 00110100 0....... ........ */
            /* la.decode:736 */
            return LISA_FLDX_D;
        case 0x70:
            /* 00111000 00111000 0....... ........ */
            /* la.decode:737 */
            return LISA_FSTX_S;
        case 0x78:
            /* 00111000 00111100 0....... ........ */
            /* la.decode:738 */
            return LISA_FSTX_D;
        case 0x80:
            /* 00111000 01000000 0....... ........ */
            /* la.decode:739 */
            return LISA_VLDX;
        case 0x88:
            /* 00111000 01000100 0....... ........ */
            /* la.decode:740 */
            return LISA_VSTX;
        case 0x90:
            /* 00111000 01001000 0....... ........ */
            /* la.decode:741 */
            return LISA_XVLDX;
        case 0x98:
            /* 00111000 01001100 0....... ........ */
            /* la.decode:742 */
            return LISA_XVSTX;
        case 0xc0:
            /* 00111000 01100000 0....... ........ */
            /* la.decode:743 */
            return LISA_AMSWAP_W;
        case 0xc1:
            /* 00111000 01100000 1....... ........ */
            /* la.decode:744 */
            return LISA_AMSWAP_D;
        case 0xc2:
            /* 00111000 01100001 0....... ........ */
            /* la.decode:745 */
            return LISA_AMADD_W;
        case 0xc3:
            /* 00111000 01100001 1....... ........ */
            /* la.decode:746 */
            return LISA_AMADD_D;
        case 0xc4:
            /* 00111000 01100010 0....... ........ */
            /* la.decode:747 */
            return LISA_AMAND_W;
        case 0xc5:
            /* 00111000 01100010 1....... ........ */
            /* la.decode:748 */
            return LISA_AMAND_D;
        case 0xc6:
            /* 00111000 01100011 0....... ........ */
            /* la.decode:749 */
            return LISA_AMOR_W;
        case 0xc7:
            /* 00111000 01100011 1....... ........ */
            /* la.decode:750 */
            return LISA_AMOR_D;
        case 0xc8:
            /* 00111000 01100100 0....... ........ */
            /* la.decode:751 */
            return LISA_AMXOR_W;
        case 0xc9:
            /* 00111000 01100100 1....... ........ */
            /* la.decode:752 */
            return LISA_AMXOR_D;
        case 0xca:
            /* 00111000 01100101 0....... ........ */
            /* la.decode:753 */
            return LISA_AMMAX_W;
        case 0xcb:
            /* 00111000 01100101 1....... ........ */
            /* la.decode:754 */
            return LISA_AMMAX_D;
        case 0xcc:
            /* 00111000 01100110 0....... ........ */
            /* la.decode:755 */
            return LISA_AMMIN_W;
        case 0xcd:
            /* 00111000 01100110 1....... ........ */
            /* la.decode:756 */
            return LISA_AMMIN_D;
        case 0xce:
            /* 00111000 01100111 0....... ........ */
            /* la.decode:757 */
            return LISA_AMMAX_WU;
        case 0xcf:
            /* 00111000 01100111 1....... ........ */
            /* la.decode:758 */
            return LISA_AMMAX_DU;
        case 0xd0:
            /* 00111000 01101000 0....... ........ */
            /* la.decode:759 */
            return LISA_AMMIN_WU;
        case 0xd1:
            /* 00111000 01101000 1....... ........ */
            /* la.decode:760 */
            return LISA_AMMIN_DU;
        case 0xd2:
            /* 00111000 01101001 0....... ........ */
            /* la.decode:761 */
            return LISA_AMSWAP_DB_W;
        case 0xd3:
            /* 00111000 01101001 1....... ........ */
            /* la.decode:762 */
            return LISA_AMSWAP_DB_D;
        case 0xd4:
            /* 00111000 01101010 0....... ........ */
            /* la.decode:763 */
            return LISA_AMADD_DB_W;
        case 0xd5:
            /* 00111000 01101010 1....... ........ */
            /* la.decode:764 */
            return LISA_AMADD_DB_D;
        case 0xd6:
            /* 00111000 01101011 0....... ........ */
            /* la.decode:765 */
            return LISA_AMAND_DB_W;
        case 0xd7:
            /* 00111000 01101011 1....... ........ */
            /* la.decode:766 */
            return LISA_AMAND_DB_D;
        case 0xd8:
            /* 00111000 01101100 0....... ........ */
            /* la.decode:767 */
            return LISA_AMOR_DB_W;
        case 0xd9:
            /* 00111000 01101100 1....... ........ */
            /* la.decode:768 */
            return LISA_AMOR_DB_D;
        case 0xda:
            /* 00111000 01101101 0....... ........ */
            /* la.decode:769 */
            return LISA_AMXOR_DB_W;
        case 0xdb:
            /* 00111000 01101101 1....... ........ */
            /* la.decode:770 */
            return LISA_AMXOR_DB_D;
        case 0xdc:
            /* 00111000 01101110 0....... ........ */
            /* la.decode:771 */
            return LISA_AMMAX_DB_W;
        case 0xdd:
            /* 00111000 01101110 1....... ........ */
            /* la.decode:772 */
            return LISA_AMMAX_DB_D;
        case 0xde:
            /* 00111000 01101111 0....... ........ */
            /* la.decode:773 */
            return LISA_AMMIN_DB_W;
        case 0xdf:
            /* 00111000 01101111 1....... ........ */
            /* la.decode:774 */
            return LISA_AMMIN_DB_D;
        case 0xe0:
            /* 00111000 01110000 0....... ........ */
            /* la.decode:775 */
            return LISA_AMMAX_DB_WU;
        case 0xe1:
            /* 00111000 01110000 1....... ........ */
            /* la.decode:776 */
            return LISA_AMMAX_DB_DU;
        case 0xe2:
            /* 00111000 01110001 0....... ........ */
            /* la.decode:777 */
            return LISA_AMMIN_DB_WU;
        case 0xe3:
            /* 00111000 01110001 1....... ........ */
            /* la.decode:778 */
            return LISA_AMMIN_DB_DU;
        case 0xe4:
            /* 00111000 01110010 0....... ........ */
            /* la.decode:779 */
            return LISA_DBAR;
        case 0xe5:
            /* 00111000 01110010 1....... ........ */
            /* la.decode:780 */
            return LISA_IBAR;
        case 0xe8:
            /* 00111000 01110100 0....... ........ */
            /* la.decode:781 */
            return LISA_FLDGT_S;
        case 0xe9:
            /* 00111000 01110100 1....... ........ */
            /* la.decode:782 */
            return LISA_FLDGT_D;
        case 0xea:
            /* 00111000 01110101 0....... ........ */
            /* la.decode:783 */
            return LISA_FLDLE_S;
        case 0xeb:
            /* 00111000 01110101 1....... ........ */
            /* la.decode:784 */
            return LISA_FLDLE_D;
        case 0xec:
            /* 00111000 01110110 0....... ........ */
            /* la.decode:785 */
            return LISA_FSTGT_S;
        case 0xed:
            /* 00111000 01110110 1....... ........ */
            /* la.decode:786 */
            return LISA_FSTGT_D;
        case 0xee:
            /* 00111000 01110111 0....... ........ */
            /* la.decode:787 */
            return LISA_FSTLE_S;
        case 0xef:
            /* 00111000 01110111 1....... ........ */
            /* la.decode:788 */
            return LISA_FSTLE_D;
        case 0xf0:
            /* 00111000 01111000 0....... ........ */
            /* la.decode:789 */
            return LISA_LDGT_B;
        case 0xf1:
            /* 00111000 01111000 1....... ........ */
            /* la.decode:790 */
            return LISA_LDGT_H;
        case 0xf2:
            /* 00111000 01111001 0....... ........ */
            /* la.decode:791 */
            return LISA_LDGT_W;
        case 0xf3:
            /* 00111000 01111001 1....... ........ */
            /* la.decode:792 */
            return LISA_LDGT_D;
        case 0xf4:
            /* 00111000 01111010 0....... ........ */
            /* la.decode:793 */
            return LISA_LDLE_B;
        case 0xf5:
            /* 00111000 01111010 1....... ........ */
            /* la.decode:794 */
            return LISA_LDLE_H;
        case 0xf6:
            /* 00111000 01111011 0....... ........ */
            /* la.decode:795 */
            return LISA_LDLE_W;
        case 0xf7:
            /* 00111000 01111011 1....... ........ */
            /* la.decode:796 */
            return LISA_LDLE_D;
        case 0xf8:
            /* 00111000 01111100 0....... ........ */
            /* la.decode:797 */
            return LISA_STGT_B;
        case 0xf9:
            /* 00111000 01111100 1....... ........ */
            /* la.decode:798 */
            return LISA_STGT_H;
        case 0xfa:
            /* 00111000 01111101 0....... ........ */
            /* la.decode:799 */
            return LISA_STGT_W;
        case 0xfb:
            /* 00111000 01111101 1....... ........ */
            /* la.decode:800 */
            return LISA_STGT_D;
        case 0xfc:
            /* 00111000 01111110 0....... ........ */
            /* la.decode:801 */
            return LISA_STLE_B;
        case 0xfd:
            /* 00111000 01111110 1....... ........ */
            /* la.decode:802 */
            return LISA_STLE_H;
        case 0xfe:
            /* 00111000 01111111 0....... ........ */
            /* la.decode:803 */
            return LISA_STLE_W;
        case 0xff:
            /* 00111000 01111111 1....... ........ */
            /* la.decode:804 */
            return LISA_STLE_D;
        }
        return LISA_INVALID;
    case 0x10:
        /* 010000.. ........ ........ ........ */
        /* la.decode:805 */
        return LISA_BEQZ;
    case 0x11:
        /* 010001.. ........ ........ ........ */
        /* la.decode:806 */
        return LISA_BNEZ;
    case 0x12:
        /* 010010.. ........ ........ ........ */
        switch ((insn >> 8) & 0x3) {
        case 0x0:
            /* 010010.. ........ ......00 ........ */
            /* la.decode:807 */
            return LISA_BCEQZ;
        case 0x1:
            /* 010010.. ........ ......01 ........ */
            /* la.decode:808 */
            return LISA_BCNEZ;
        case 0x2:
            /* 010010.. ........ ......10 ........ */
            switch ((insn >> 5) & 0x7) {
            case 0x0:
                /* 010010.. ........ ......10 000..... */
                /* la.decode:809 */
                return LISA_JISCR0;
            }
            return LISA_INVALID;
        case 0x3:
            /* 010010.. ........ ......11 ........ */
            switch ((insn >> 5) & 0x7) {
            case 0x0:
                /* 010010.. ........ ......11 000..... */
                /* la.decode:810 */
                return LISA_JISCR1;
            }
            return LISA_INVALID;
        }
        return LISA_INVALID;
    case 0x13:
        /* 010011.. ........ ........ ........ */
        /* la.decode:811 */
        return LISA_JIRL;
    case 0x14:
        /* 010100.. ........ ........ ........ */
        /* la.decode:812 */
        return LISA_B;
    case 0x15:
        /* 010101.. ........ ........ ........ */
        /* la.decode:813 */
        return LISA_BL;
    case 0x16:
        /* 010110.. ........ ........ ........ */
        /* la.decode:814 */
        return LISA_BEQ;
    case 0x17:
        /* 010111.. ........ ........ ........ */
        /* la.decode:815 */
        return LISA_BNE;
    case 0x18:
        /* 011000.. ........ ........ ........ */
        /* la.decode:816 */
        return LISA_BLT;
    case 0x19:
        /* 011001.. ........ ........ ........ */
        /* la.decode:817 */
        return LISA_BGE;
    case 0x1a:
        /* 011010.. ........ ........ ........ */
        /* la.decode:818 */
        return LISA_BLTU;
    case 0x1b:
        /* 011011.. ........ ........ ........ */
        /* la.decode:819 */
        return LISA_BGEU;
    case 0x1c:
        /* 011100.. ........ ........ ........ */
        switch ((insn >> 18) & 0xff) {
        case 0x0:
            /* 01110000 000000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00000000 0....... ........ */
                /* la.decode:820 */
                return LISA_VSEQ_B;
            case 0x1:
                /* 01110000 00000000 1....... ........ */
                /* la.decode:821 */
                return LISA_VSEQ_H;
            case 0x2:
                /* 01110000 00000001 0....... ........ */
                /* la.decode:822 */
                return LISA_VSEQ_W;
            case 0x3:
                /* 01110000 00000001 1....... ........ */
                /* la.decode:823 */
                return LISA_VSEQ_D;
            case 0x4:
                /* 01110000 00000010 0....... ........ */
                /* la.decode:824 */
                return LISA_VSLE_B;
            case 0x5:
                /* 01110000 00000010 1....... ........ */
                /* la.decode:825 */
                return LISA_VSLE_H;
            case 0x6:
                /* 01110000 00000011 0....... ........ */
                /* la.decode:826 */
                return LISA_VSLE_W;
            case 0x7:
                /* 01110000 00000011 1....... ........ */
                /* la.decode:827 */
                return LISA_VSLE_D;
            }
            return LISA_INVALID;
        case 0x1:
            /* 01110000 000001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00000100 0....... ........ */
                /* la.decode:828 */
                return LISA_VSLE_BU;
            case 0x1:
                /* 01110000 00000100 1....... ........ */
                /* la.decode:829 */
                return LISA_VSLE_HU;
            case 0x2:
                /* 01110000 00000101 0....... ........ */
                /* la.decode:830 */
                return LISA_VSLE_WU;
            case 0x3:
                /* 01110000 00000101 1....... ........ */
                /* la.decode:831 */
                return LISA_VSLE_DU;
            case 0x4:
                /* 01110000 00000110 0....... ........ */
                /* la.decode:832 */
                return LISA_VSLT_B;
            case 0x5:
                /* 01110000 00000110 1....... ........ */
                /* la.decode:833 */
                return LISA_VSLT_H;
            case 0x6:
                /* 01110000 00000111 0....... ........ */
                /* la.decode:834 */
                return LISA_VSLT_W;
            case 0x7:
                /* 01110000 00000111 1....... ........ */
                /* la.decode:835 */
                return LISA_VSLT_D;
            }
            return LISA_INVALID;
        case 0x2:
            /* 01110000 000010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00001000 0....... ........ */
                /* la.decode:836 */
                return LISA_VSLT_BU;
            case 0x1:
                /* 01110000 00001000 1....... ........ */
                /* la.decode:837 */
                return LISA_VSLT_HU;
            case 0x2:
                /* 01110000 00001001 0....... ........ */
                /* la.decode:838 */
                return LISA_VSLT_WU;
            case 0x3:
                /* 01110000 00001001 1....... ........ */
                /* la.decode:839 */
                return LISA_VSLT_DU;
            case 0x4:
                /* 01110000 00001010 0....... ........ */
                /* la.decode:840 */
                return LISA_VADD_B;
            case 0x5:
                /* 01110000 00001010 1....... ........ */
                /* la.decode:841 */
                return LISA_VADD_H;
            case 0x6:
                /* 01110000 00001011 0....... ........ */
                /* la.decode:842 */
                return LISA_VADD_W;
            case 0x7:
                /* 01110000 00001011 1....... ........ */
                /* la.decode:843 */
                return LISA_VADD_D;
            }
            return LISA_INVALID;
        case 0x3:
            /* 01110000 000011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00001100 0....... ........ */
                /* la.decode:844 */
                return LISA_VSUB_B;
            case 0x1:
                /* 01110000 00001100 1....... ........ */
                /* la.decode:845 */
                return LISA_VSUB_H;
            case 0x2:
                /* 01110000 00001101 0....... ........ */
                /* la.decode:846 */
                return LISA_VSUB_W;
            case 0x3:
                /* 01110000 00001101 1....... ........ */
                /* la.decode:847 */
                return LISA_VSUB_D;
            case 0x4:
                /* 01110000 00001110 0....... ........ */
                /* la.decode:848 */
                return LISA_VADDW_H_H_B;
            case 0x5:
                /* 01110000 00001110 1....... ........ */
                /* la.decode:849 */
                return LISA_VADDW_W_W_H;
            case 0x6:
                /* 01110000 00001111 0....... ........ */
                /* la.decode:850 */
                return LISA_VADDW_D_D_W;
            case 0x7:
                /* 01110000 00001111 1....... ........ */
                /* la.decode:851 */
                return LISA_VADDW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x4:
            /* 01110000 000100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00010000 0....... ........ */
                /* la.decode:852 */
                return LISA_VADDW_H_H_BU;
            case 0x1:
                /* 01110000 00010000 1....... ........ */
                /* la.decode:853 */
                return LISA_VADDW_W_W_HU;
            case 0x2:
                /* 01110000 00010001 0....... ........ */
                /* la.decode:854 */
                return LISA_VADDW_D_D_WU;
            case 0x3:
                /* 01110000 00010001 1....... ........ */
                /* la.decode:855 */
                return LISA_VADDW_Q_Q_DU;
            case 0x4:
                /* 01110000 00010010 0....... ........ */
                /* la.decode:856 */
                return LISA_VSUBW_H_H_B;
            case 0x5:
                /* 01110000 00010010 1....... ........ */
                /* la.decode:857 */
                return LISA_VSUBW_W_W_H;
            case 0x6:
                /* 01110000 00010011 0....... ........ */
                /* la.decode:858 */
                return LISA_VSUBW_D_D_W;
            case 0x7:
                /* 01110000 00010011 1....... ........ */
                /* la.decode:859 */
                return LISA_VSUBW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x5:
            /* 01110000 000101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00010100 0....... ........ */
                /* la.decode:860 */
                return LISA_VSUBW_H_H_BU;
            case 0x1:
                /* 01110000 00010100 1....... ........ */
                /* la.decode:861 */
                return LISA_VSUBW_W_W_HU;
            case 0x2:
                /* 01110000 00010101 0....... ........ */
                /* la.decode:862 */
                return LISA_VSUBW_D_D_WU;
            case 0x3:
                /* 01110000 00010101 1....... ........ */
                /* la.decode:863 */
                return LISA_VSUBW_Q_Q_DU;
            case 0x4:
                /* 01110000 00010110 0....... ........ */
                /* la.decode:864 */
                return LISA_VSADDW_H_H_B;
            case 0x5:
                /* 01110000 00010110 1....... ........ */
                /* la.decode:865 */
                return LISA_VSADDW_W_W_H;
            case 0x6:
                /* 01110000 00010111 0....... ........ */
                /* la.decode:866 */
                return LISA_VSADDW_D_D_W;
            case 0x7:
                /* 01110000 00010111 1....... ........ */
                /* la.decode:867 */
                return LISA_VSADDW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x6:
            /* 01110000 000110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00011000 0....... ........ */
                /* la.decode:868 */
                return LISA_VSADDW_HU_HU_BU;
            case 0x1:
                /* 01110000 00011000 1....... ........ */
                /* la.decode:869 */
                return LISA_VSADDW_WU_WU_HU;
            case 0x2:
                /* 01110000 00011001 0....... ........ */
                /* la.decode:870 */
                return LISA_VSADDW_DU_DU_WU;
            case 0x3:
                /* 01110000 00011001 1....... ........ */
                /* la.decode:871 */
                return LISA_VSADDW_QU_QU_DU;
            case 0x4:
                /* 01110000 00011010 0....... ........ */
                /* la.decode:872 */
                return LISA_VSSUBW_H_H_B;
            case 0x5:
                /* 01110000 00011010 1....... ........ */
                /* la.decode:873 */
                return LISA_VSSUBW_W_W_H;
            case 0x6:
                /* 01110000 00011011 0....... ........ */
                /* la.decode:874 */
                return LISA_VSSUBW_D_D_W;
            case 0x7:
                /* 01110000 00011011 1....... ........ */
                /* la.decode:875 */
                return LISA_VSSUBW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x7:
            /* 01110000 000111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00011100 0....... ........ */
                /* la.decode:876 */
                return LISA_VSSUBW_HU_HU_BU;
            case 0x1:
                /* 01110000 00011100 1....... ........ */
                /* la.decode:877 */
                return LISA_VSSUBW_WU_WU_HU;
            case 0x2:
                /* 01110000 00011101 0....... ........ */
                /* la.decode:878 */
                return LISA_VSSUBW_DU_DU_WU;
            case 0x3:
                /* 01110000 00011101 1....... ........ */
                /* la.decode:879 */
                return LISA_VSSUBW_QU_QU_DU;
            case 0x4:
                /* 01110000 00011110 0....... ........ */
                /* la.decode:880 */
                return LISA_VADDWEV_H_B;
            case 0x5:
                /* 01110000 00011110 1....... ........ */
                /* la.decode:881 */
                return LISA_VADDWEV_W_H;
            case 0x6:
                /* 01110000 00011111 0....... ........ */
                /* la.decode:882 */
                return LISA_VADDWEV_D_W;
            case 0x7:
                /* 01110000 00011111 1....... ........ */
                /* la.decode:883 */
                return LISA_VADDWEV_Q_D;
            }
            return LISA_INVALID;
        case 0x8:
            /* 01110000 001000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00100000 0....... ........ */
                /* la.decode:884 */
                return LISA_VSUBWEV_H_B;
            case 0x1:
                /* 01110000 00100000 1....... ........ */
                /* la.decode:885 */
                return LISA_VSUBWEV_W_H;
            case 0x2:
                /* 01110000 00100001 0....... ........ */
                /* la.decode:886 */
                return LISA_VSUBWEV_D_W;
            case 0x3:
                /* 01110000 00100001 1....... ........ */
                /* la.decode:887 */
                return LISA_VSUBWEV_Q_D;
            case 0x4:
                /* 01110000 00100010 0....... ........ */
                /* la.decode:888 */
                return LISA_VADDWOD_H_B;
            case 0x5:
                /* 01110000 00100010 1....... ........ */
                /* la.decode:889 */
                return LISA_VADDWOD_W_H;
            case 0x6:
                /* 01110000 00100011 0....... ........ */
                /* la.decode:890 */
                return LISA_VADDWOD_D_W;
            case 0x7:
                /* 01110000 00100011 1....... ........ */
                /* la.decode:891 */
                return LISA_VADDWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x9:
            /* 01110000 001001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00100100 0....... ........ */
                /* la.decode:892 */
                return LISA_VSUBWOD_H_B;
            case 0x1:
                /* 01110000 00100100 1....... ........ */
                /* la.decode:893 */
                return LISA_VSUBWOD_W_H;
            case 0x2:
                /* 01110000 00100101 0....... ........ */
                /* la.decode:894 */
                return LISA_VSUBWOD_D_W;
            case 0x3:
                /* 01110000 00100101 1....... ........ */
                /* la.decode:895 */
                return LISA_VSUBWOD_Q_D;
            case 0x4:
                /* 01110000 00100110 0....... ........ */
                /* la.decode:896 */
                return LISA_VADDWL_H_B;
            case 0x5:
                /* 01110000 00100110 1....... ........ */
                /* la.decode:897 */
                return LISA_VADDWL_W_H;
            case 0x6:
                /* 01110000 00100111 0....... ........ */
                /* la.decode:898 */
                return LISA_VADDWL_D_W;
            case 0x7:
                /* 01110000 00100111 1....... ........ */
                /* la.decode:899 */
                return LISA_VADDWL_Q_D;
            }
            return LISA_INVALID;
        case 0xa:
            /* 01110000 001010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00101000 0....... ........ */
                /* la.decode:900 */
                return LISA_VSUBWL_H_B;
            case 0x1:
                /* 01110000 00101000 1....... ........ */
                /* la.decode:901 */
                return LISA_VSUBWL_W_H;
            case 0x2:
                /* 01110000 00101001 0....... ........ */
                /* la.decode:902 */
                return LISA_VSUBWL_D_W;
            case 0x3:
                /* 01110000 00101001 1....... ........ */
                /* la.decode:903 */
                return LISA_VSUBWL_Q_D;
            case 0x4:
                /* 01110000 00101010 0....... ........ */
                /* la.decode:904 */
                return LISA_VADDWH_H_B;
            case 0x5:
                /* 01110000 00101010 1....... ........ */
                /* la.decode:905 */
                return LISA_VADDWH_W_H;
            case 0x6:
                /* 01110000 00101011 0....... ........ */
                /* la.decode:906 */
                return LISA_VADDWH_D_W;
            case 0x7:
                /* 01110000 00101011 1....... ........ */
                /* la.decode:907 */
                return LISA_VADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0xb:
            /* 01110000 001011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00101100 0....... ........ */
                /* la.decode:908 */
                return LISA_VSUBWH_H_B;
            case 0x1:
                /* 01110000 00101100 1....... ........ */
                /* la.decode:909 */
                return LISA_VSUBWH_W_H;
            case 0x2:
                /* 01110000 00101101 0....... ........ */
                /* la.decode:910 */
                return LISA_VSUBWH_D_W;
            case 0x3:
                /* 01110000 00101101 1....... ........ */
                /* la.decode:911 */
                return LISA_VSUBWH_Q_D;
            case 0x4:
                /* 01110000 00101110 0....... ........ */
                /* la.decode:912 */
                return LISA_VADDWEV_H_BU;
            case 0x5:
                /* 01110000 00101110 1....... ........ */
                /* la.decode:913 */
                return LISA_VADDWEV_W_HU;
            case 0x6:
                /* 01110000 00101111 0....... ........ */
                /* la.decode:914 */
                return LISA_VADDWEV_D_WU;
            case 0x7:
                /* 01110000 00101111 1....... ........ */
                /* la.decode:915 */
                return LISA_VADDWEV_Q_DU;
            }
            return LISA_INVALID;
        case 0xc:
            /* 01110000 001100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00110000 0....... ........ */
                /* la.decode:916 */
                return LISA_VSUBWEV_H_BU;
            case 0x1:
                /* 01110000 00110000 1....... ........ */
                /* la.decode:917 */
                return LISA_VSUBWEV_W_HU;
            case 0x2:
                /* 01110000 00110001 0....... ........ */
                /* la.decode:918 */
                return LISA_VSUBWEV_D_WU;
            case 0x3:
                /* 01110000 00110001 1....... ........ */
                /* la.decode:919 */
                return LISA_VSUBWEV_Q_DU;
            case 0x4:
                /* 01110000 00110010 0....... ........ */
                /* la.decode:920 */
                return LISA_VADDWOD_H_BU;
            case 0x5:
                /* 01110000 00110010 1....... ........ */
                /* la.decode:921 */
                return LISA_VADDWOD_W_HU;
            case 0x6:
                /* 01110000 00110011 0....... ........ */
                /* la.decode:922 */
                return LISA_VADDWOD_D_WU;
            case 0x7:
                /* 01110000 00110011 1....... ........ */
                /* la.decode:923 */
                return LISA_VADDWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0xd:
            /* 01110000 001101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00110100 0....... ........ */
                /* la.decode:924 */
                return LISA_VSUBWOD_H_BU;
            case 0x1:
                /* 01110000 00110100 1....... ........ */
                /* la.decode:925 */
                return LISA_VSUBWOD_W_HU;
            case 0x2:
                /* 01110000 00110101 0....... ........ */
                /* la.decode:926 */
                return LISA_VSUBWOD_D_WU;
            case 0x3:
                /* 01110000 00110101 1....... ........ */
                /* la.decode:927 */
                return LISA_VSUBWOD_Q_DU;
            case 0x4:
                /* 01110000 00110110 0....... ........ */
                /* la.decode:928 */
                return LISA_VADDWL_H_BU;
            case 0x5:
                /* 01110000 00110110 1....... ........ */
                /* la.decode:929 */
                return LISA_VADDWL_W_HU;
            case 0x6:
                /* 01110000 00110111 0....... ........ */
                /* la.decode:930 */
                return LISA_VADDWL_D_WU;
            case 0x7:
                /* 01110000 00110111 1....... ........ */
                /* la.decode:931 */
                return LISA_VADDWL_Q_DU;
            }
            return LISA_INVALID;
        case 0xe:
            /* 01110000 001110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00111000 0....... ........ */
                /* la.decode:932 */
                return LISA_VSUBWL_H_BU;
            case 0x1:
                /* 01110000 00111000 1....... ........ */
                /* la.decode:933 */
                return LISA_VSUBWL_W_HU;
            case 0x2:
                /* 01110000 00111001 0....... ........ */
                /* la.decode:934 */
                return LISA_VSUBWL_D_WU;
            case 0x3:
                /* 01110000 00111001 1....... ........ */
                /* la.decode:935 */
                return LISA_VSUBWL_Q_DU;
            case 0x4:
                /* 01110000 00111010 0....... ........ */
                /* la.decode:936 */
                return LISA_VADDWH_H_BU;
            case 0x5:
                /* 01110000 00111010 1....... ........ */
                /* la.decode:937 */
                return LISA_VADDWH_W_HU;
            case 0x6:
                /* 01110000 00111011 0....... ........ */
                /* la.decode:938 */
                return LISA_VADDWH_D_WU;
            case 0x7:
                /* 01110000 00111011 1....... ........ */
                /* la.decode:939 */
                return LISA_VADDWH_Q_DU;
            }
            return LISA_INVALID;
        case 0xf:
            /* 01110000 001111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 00111100 0....... ........ */
                /* la.decode:940 */
                return LISA_VSUBWH_H_BU;
            case 0x1:
                /* 01110000 00111100 1....... ........ */
                /* la.decode:941 */
                return LISA_VSUBWH_W_HU;
            case 0x2:
                /* 01110000 00111101 0....... ........ */
                /* la.decode:942 */
                return LISA_VSUBWH_D_WU;
            case 0x3:
                /* 01110000 00111101 1....... ........ */
                /* la.decode:943 */
                return LISA_VSUBWH_Q_DU;
            case 0x4:
                /* 01110000 00111110 0....... ........ */
                /* la.decode:944 */
                return LISA_VADDWEV_H_BU_B;
            case 0x5:
                /* 01110000 00111110 1....... ........ */
                /* la.decode:945 */
                return LISA_VADDWEV_W_HU_H;
            case 0x6:
                /* 01110000 00111111 0....... ........ */
                /* la.decode:946 */
                return LISA_VADDWEV_D_WU_W;
            case 0x7:
                /* 01110000 00111111 1....... ........ */
                /* la.decode:947 */
                return LISA_VADDWEV_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x10:
            /* 01110000 010000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01000000 0....... ........ */
                /* la.decode:948 */
                return LISA_VADDWOD_H_BU_B;
            case 0x1:
                /* 01110000 01000000 1....... ........ */
                /* la.decode:949 */
                return LISA_VADDWOD_W_HU_H;
            case 0x2:
                /* 01110000 01000001 0....... ........ */
                /* la.decode:950 */
                return LISA_VADDWOD_D_WU_W;
            case 0x3:
                /* 01110000 01000001 1....... ........ */
                /* la.decode:951 */
                return LISA_VADDWOD_Q_DU_D;
            case 0x4:
                /* 01110000 01000010 0....... ........ */
                /* la.decode:952 */
                return LISA_VADDWL_H_BU_B;
            case 0x5:
                /* 01110000 01000010 1....... ........ */
                /* la.decode:953 */
                return LISA_VADDWL_W_HU_H;
            case 0x6:
                /* 01110000 01000011 0....... ........ */
                /* la.decode:954 */
                return LISA_VADDWL_D_WU_W;
            case 0x7:
                /* 01110000 01000011 1....... ........ */
                /* la.decode:955 */
                return LISA_VADDWL_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x11:
            /* 01110000 010001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01000100 0....... ........ */
                /* la.decode:956 */
                return LISA_VADDWH_H_BU_B;
            case 0x1:
                /* 01110000 01000100 1....... ........ */
                /* la.decode:957 */
                return LISA_VADDWH_W_HU_H;
            case 0x2:
                /* 01110000 01000101 0....... ........ */
                /* la.decode:958 */
                return LISA_VADDWH_D_WU_W;
            case 0x3:
                /* 01110000 01000101 1....... ........ */
                /* la.decode:959 */
                return LISA_VADDWH_Q_DU_D;
            case 0x4:
                /* 01110000 01000110 0....... ........ */
                /* la.decode:960 */
                return LISA_VSADD_B;
            case 0x5:
                /* 01110000 01000110 1....... ........ */
                /* la.decode:961 */
                return LISA_VSADD_H;
            case 0x6:
                /* 01110000 01000111 0....... ........ */
                /* la.decode:962 */
                return LISA_VSADD_W;
            case 0x7:
                /* 01110000 01000111 1....... ........ */
                /* la.decode:963 */
                return LISA_VSADD_D;
            }
            return LISA_INVALID;
        case 0x12:
            /* 01110000 010010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01001000 0....... ........ */
                /* la.decode:964 */
                return LISA_VSSUB_B;
            case 0x1:
                /* 01110000 01001000 1....... ........ */
                /* la.decode:965 */
                return LISA_VSSUB_H;
            case 0x2:
                /* 01110000 01001001 0....... ........ */
                /* la.decode:966 */
                return LISA_VSSUB_W;
            case 0x3:
                /* 01110000 01001001 1....... ........ */
                /* la.decode:967 */
                return LISA_VSSUB_D;
            case 0x4:
                /* 01110000 01001010 0....... ........ */
                /* la.decode:968 */
                return LISA_VSADD_BU;
            case 0x5:
                /* 01110000 01001010 1....... ........ */
                /* la.decode:969 */
                return LISA_VSADD_HU;
            case 0x6:
                /* 01110000 01001011 0....... ........ */
                /* la.decode:970 */
                return LISA_VSADD_WU;
            case 0x7:
                /* 01110000 01001011 1....... ........ */
                /* la.decode:971 */
                return LISA_VSADD_DU;
            }
            return LISA_INVALID;
        case 0x13:
            /* 01110000 010011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01001100 0....... ........ */
                /* la.decode:972 */
                return LISA_VSSUB_BU;
            case 0x1:
                /* 01110000 01001100 1....... ........ */
                /* la.decode:973 */
                return LISA_VSSUB_HU;
            case 0x2:
                /* 01110000 01001101 0....... ........ */
                /* la.decode:974 */
                return LISA_VSSUB_WU;
            case 0x3:
                /* 01110000 01001101 1....... ........ */
                /* la.decode:975 */
                return LISA_VSSUB_DU;
            case 0x4:
                /* 01110000 01001110 0....... ........ */
                /* la.decode:976 */
                return LISA_VSSUB_BU_BU_B;
            case 0x5:
                /* 01110000 01001110 1....... ........ */
                /* la.decode:977 */
                return LISA_VSSUB_HU_HU_H;
            case 0x6:
                /* 01110000 01001111 0....... ........ */
                /* la.decode:978 */
                return LISA_VSSUB_WU_WU_W;
            case 0x7:
                /* 01110000 01001111 1....... ........ */
                /* la.decode:979 */
                return LISA_VSSUB_DU_DU_D;
            }
            return LISA_INVALID;
        case 0x14:
            /* 01110000 010100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01010000 0....... ........ */
                /* la.decode:980 */
                return LISA_VSSUB_BU_B_BU;
            case 0x1:
                /* 01110000 01010000 1....... ........ */
                /* la.decode:981 */
                return LISA_VSSUB_HU_H_HU;
            case 0x2:
                /* 01110000 01010001 0....... ........ */
                /* la.decode:982 */
                return LISA_VSSUB_WU_W_WU;
            case 0x3:
                /* 01110000 01010001 1....... ........ */
                /* la.decode:983 */
                return LISA_VSSUB_DU_D_DU;
            case 0x4:
                /* 01110000 01010010 0....... ........ */
                /* la.decode:984 */
                return LISA_VSSUB_B_BU_BU;
            case 0x5:
                /* 01110000 01010010 1....... ........ */
                /* la.decode:985 */
                return LISA_VSSUB_H_HU_HU;
            case 0x6:
                /* 01110000 01010011 0....... ........ */
                /* la.decode:986 */
                return LISA_VSSUB_W_WU_WU;
            case 0x7:
                /* 01110000 01010011 1....... ........ */
                /* la.decode:987 */
                return LISA_VSSUB_D_DU_DU;
            }
            return LISA_INVALID;
        case 0x15:
            /* 01110000 010101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01010100 0....... ........ */
                /* la.decode:988 */
                return LISA_VHADDW_H_B;
            case 0x1:
                /* 01110000 01010100 1....... ........ */
                /* la.decode:989 */
                return LISA_VHADDW_W_H;
            case 0x2:
                /* 01110000 01010101 0....... ........ */
                /* la.decode:990 */
                return LISA_VHADDW_D_W;
            case 0x3:
                /* 01110000 01010101 1....... ........ */
                /* la.decode:991 */
                return LISA_VHADDW_Q_D;
            case 0x4:
                /* 01110000 01010110 0....... ........ */
                /* la.decode:992 */
                return LISA_VHSUBW_H_B;
            case 0x5:
                /* 01110000 01010110 1....... ........ */
                /* la.decode:993 */
                return LISA_VHSUBW_W_H;
            case 0x6:
                /* 01110000 01010111 0....... ........ */
                /* la.decode:994 */
                return LISA_VHSUBW_D_W;
            case 0x7:
                /* 01110000 01010111 1....... ........ */
                /* la.decode:995 */
                return LISA_VHSUBW_Q_D;
            }
            return LISA_INVALID;
        case 0x16:
            /* 01110000 010110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01011000 0....... ........ */
                /* la.decode:996 */
                return LISA_VHADDW_HU_BU;
            case 0x1:
                /* 01110000 01011000 1....... ........ */
                /* la.decode:997 */
                return LISA_VHADDW_WU_HU;
            case 0x2:
                /* 01110000 01011001 0....... ........ */
                /* la.decode:998 */
                return LISA_VHADDW_DU_WU;
            case 0x3:
                /* 01110000 01011001 1....... ........ */
                /* la.decode:999 */
                return LISA_VHADDW_QU_DU;
            case 0x4:
                /* 01110000 01011010 0....... ........ */
                /* la.decode:1000 */
                return LISA_VHSUBW_HU_BU;
            case 0x5:
                /* 01110000 01011010 1....... ........ */
                /* la.decode:1001 */
                return LISA_VHSUBW_WU_HU;
            case 0x6:
                /* 01110000 01011011 0....... ........ */
                /* la.decode:1002 */
                return LISA_VHSUBW_DU_WU;
            case 0x7:
                /* 01110000 01011011 1....... ........ */
                /* la.decode:1003 */
                return LISA_VHSUBW_QU_DU;
            }
            return LISA_INVALID;
        case 0x17:
            /* 01110000 010111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01011100 0....... ........ */
                /* la.decode:1004 */
                return LISA_VADDA_B;
            case 0x1:
                /* 01110000 01011100 1....... ........ */
                /* la.decode:1005 */
                return LISA_VADDA_H;
            case 0x2:
                /* 01110000 01011101 0....... ........ */
                /* la.decode:1006 */
                return LISA_VADDA_W;
            case 0x3:
                /* 01110000 01011101 1....... ........ */
                /* la.decode:1007 */
                return LISA_VADDA_D;
            case 0x4:
                /* 01110000 01011110 0....... ........ */
                /* la.decode:1008 */
                return LISA_VSADDA_B;
            case 0x5:
                /* 01110000 01011110 1....... ........ */
                /* la.decode:1009 */
                return LISA_VSADDA_H;
            case 0x6:
                /* 01110000 01011111 0....... ........ */
                /* la.decode:1010 */
                return LISA_VSADDA_W;
            case 0x7:
                /* 01110000 01011111 1....... ........ */
                /* la.decode:1011 */
                return LISA_VSADDA_D;
            }
            return LISA_INVALID;
        case 0x18:
            /* 01110000 011000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01100000 0....... ........ */
                /* la.decode:1012 */
                return LISA_VABSD_B;
            case 0x1:
                /* 01110000 01100000 1....... ........ */
                /* la.decode:1013 */
                return LISA_VABSD_H;
            case 0x2:
                /* 01110000 01100001 0....... ........ */
                /* la.decode:1014 */
                return LISA_VABSD_W;
            case 0x3:
                /* 01110000 01100001 1....... ........ */
                /* la.decode:1015 */
                return LISA_VABSD_D;
            case 0x4:
                /* 01110000 01100010 0....... ........ */
                /* la.decode:1016 */
                return LISA_VABSD_BU;
            case 0x5:
                /* 01110000 01100010 1....... ........ */
                /* la.decode:1017 */
                return LISA_VABSD_HU;
            case 0x6:
                /* 01110000 01100011 0....... ........ */
                /* la.decode:1018 */
                return LISA_VABSD_WU;
            case 0x7:
                /* 01110000 01100011 1....... ........ */
                /* la.decode:1019 */
                return LISA_VABSD_DU;
            }
            return LISA_INVALID;
        case 0x19:
            /* 01110000 011001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01100100 0....... ........ */
                /* la.decode:1020 */
                return LISA_VAVG_B;
            case 0x1:
                /* 01110000 01100100 1....... ........ */
                /* la.decode:1021 */
                return LISA_VAVG_H;
            case 0x2:
                /* 01110000 01100101 0....... ........ */
                /* la.decode:1022 */
                return LISA_VAVG_W;
            case 0x3:
                /* 01110000 01100101 1....... ........ */
                /* la.decode:1023 */
                return LISA_VAVG_D;
            case 0x4:
                /* 01110000 01100110 0....... ........ */
                /* la.decode:1024 */
                return LISA_VAVG_BU;
            case 0x5:
                /* 01110000 01100110 1....... ........ */
                /* la.decode:1025 */
                return LISA_VAVG_HU;
            case 0x6:
                /* 01110000 01100111 0....... ........ */
                /* la.decode:1026 */
                return LISA_VAVG_WU;
            case 0x7:
                /* 01110000 01100111 1....... ........ */
                /* la.decode:1027 */
                return LISA_VAVG_DU;
            }
            return LISA_INVALID;
        case 0x1a:
            /* 01110000 011010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01101000 0....... ........ */
                /* la.decode:1028 */
                return LISA_VAVGR_B;
            case 0x1:
                /* 01110000 01101000 1....... ........ */
                /* la.decode:1029 */
                return LISA_VAVGR_H;
            case 0x2:
                /* 01110000 01101001 0....... ........ */
                /* la.decode:1030 */
                return LISA_VAVGR_W;
            case 0x3:
                /* 01110000 01101001 1....... ........ */
                /* la.decode:1031 */
                return LISA_VAVGR_D;
            case 0x4:
                /* 01110000 01101010 0....... ........ */
                /* la.decode:1032 */
                return LISA_VAVGR_BU;
            case 0x5:
                /* 01110000 01101010 1....... ........ */
                /* la.decode:1033 */
                return LISA_VAVGR_HU;
            case 0x6:
                /* 01110000 01101011 0....... ........ */
                /* la.decode:1034 */
                return LISA_VAVGR_WU;
            case 0x7:
                /* 01110000 01101011 1....... ........ */
                /* la.decode:1035 */
                return LISA_VAVGR_DU;
            }
            return LISA_INVALID;
        case 0x1b:
            /* 01110000 011011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01101100 0....... ........ */
                /* la.decode:1036 */
                return LISA_VHALFD_B;
            case 0x1:
                /* 01110000 01101100 1....... ........ */
                /* la.decode:1037 */
                return LISA_VHALFD_H;
            case 0x2:
                /* 01110000 01101101 0....... ........ */
                /* la.decode:1038 */
                return LISA_VHALFD_W;
            case 0x3:
                /* 01110000 01101101 1....... ........ */
                /* la.decode:1039 */
                return LISA_VHALFD_D;
            case 0x4:
                /* 01110000 01101110 0....... ........ */
                /* la.decode:1040 */
                return LISA_VHALFD_BU;
            case 0x5:
                /* 01110000 01101110 1....... ........ */
                /* la.decode:1041 */
                return LISA_VHALFD_HU;
            case 0x6:
                /* 01110000 01101111 0....... ........ */
                /* la.decode:1042 */
                return LISA_VHALFD_WU;
            case 0x7:
                /* 01110000 01101111 1....... ........ */
                /* la.decode:1043 */
                return LISA_VHALFD_DU;
            }
            return LISA_INVALID;
        case 0x1c:
            /* 01110000 011100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01110000 0....... ........ */
                /* la.decode:1044 */
                return LISA_VMAX_B;
            case 0x1:
                /* 01110000 01110000 1....... ........ */
                /* la.decode:1045 */
                return LISA_VMAX_H;
            case 0x2:
                /* 01110000 01110001 0....... ........ */
                /* la.decode:1046 */
                return LISA_VMAX_W;
            case 0x3:
                /* 01110000 01110001 1....... ........ */
                /* la.decode:1047 */
                return LISA_VMAX_D;
            case 0x4:
                /* 01110000 01110010 0....... ........ */
                /* la.decode:1048 */
                return LISA_VMIN_B;
            case 0x5:
                /* 01110000 01110010 1....... ........ */
                /* la.decode:1049 */
                return LISA_VMIN_H;
            case 0x6:
                /* 01110000 01110011 0....... ........ */
                /* la.decode:1050 */
                return LISA_VMIN_W;
            case 0x7:
                /* 01110000 01110011 1....... ........ */
                /* la.decode:1051 */
                return LISA_VMIN_D;
            }
            return LISA_INVALID;
        case 0x1d:
            /* 01110000 011101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01110100 0....... ........ */
                /* la.decode:1052 */
                return LISA_VMAX_BU;
            case 0x1:
                /* 01110000 01110100 1....... ........ */
                /* la.decode:1053 */
                return LISA_VMAX_HU;
            case 0x2:
                /* 01110000 01110101 0....... ........ */
                /* la.decode:1054 */
                return LISA_VMAX_WU;
            case 0x3:
                /* 01110000 01110101 1....... ........ */
                /* la.decode:1055 */
                return LISA_VMAX_DU;
            case 0x4:
                /* 01110000 01110110 0....... ........ */
                /* la.decode:1056 */
                return LISA_VMIN_BU;
            case 0x5:
                /* 01110000 01110110 1....... ........ */
                /* la.decode:1057 */
                return LISA_VMIN_HU;
            case 0x6:
                /* 01110000 01110111 0....... ........ */
                /* la.decode:1058 */
                return LISA_VMIN_WU;
            case 0x7:
                /* 01110000 01110111 1....... ........ */
                /* la.decode:1059 */
                return LISA_VMIN_DU;
            }
            return LISA_INVALID;
        case 0x1e:
            /* 01110000 011110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01111000 0....... ........ */
                /* la.decode:1060 */
                return LISA_VMAXA_B;
            case 0x1:
                /* 01110000 01111000 1....... ........ */
                /* la.decode:1061 */
                return LISA_VMAXA_H;
            case 0x2:
                /* 01110000 01111001 0....... ........ */
                /* la.decode:1062 */
                return LISA_VMAXA_W;
            case 0x3:
                /* 01110000 01111001 1....... ........ */
                /* la.decode:1063 */
                return LISA_VMAXA_D;
            case 0x4:
                /* 01110000 01111010 0....... ........ */
                /* la.decode:1064 */
                return LISA_VMINA_B;
            case 0x5:
                /* 01110000 01111010 1....... ........ */
                /* la.decode:1065 */
                return LISA_VMINA_H;
            case 0x6:
                /* 01110000 01111011 0....... ........ */
                /* la.decode:1066 */
                return LISA_VMINA_W;
            case 0x7:
                /* 01110000 01111011 1....... ........ */
                /* la.decode:1067 */
                return LISA_VMINA_D;
            }
            return LISA_INVALID;
        case 0x1f:
            /* 01110000 011111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 01111100 0....... ........ */
                /* la.decode:1068 */
                return LISA_VSADW_H_B;
            case 0x1:
                /* 01110000 01111100 1....... ........ */
                /* la.decode:1069 */
                return LISA_VSADW_W_H;
            case 0x2:
                /* 01110000 01111101 0....... ........ */
                /* la.decode:1070 */
                return LISA_VSADW_D_W;
            case 0x4:
                /* 01110000 01111110 0....... ........ */
                /* la.decode:1071 */
                return LISA_VSADW_H_BU;
            case 0x5:
                /* 01110000 01111110 1....... ........ */
                /* la.decode:1072 */
                return LISA_VSADW_W_HU;
            case 0x6:
                /* 01110000 01111111 0....... ........ */
                /* la.decode:1073 */
                return LISA_VSADW_D_WU;
            }
            return LISA_INVALID;
        case 0x20:
            /* 01110000 100000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10000000 0....... ........ */
                /* la.decode:1074 */
                return LISA_VACCSADW_H_B;
            case 0x1:
                /* 01110000 10000000 1....... ........ */
                /* la.decode:1075 */
                return LISA_VACCSADW_W_H;
            case 0x2:
                /* 01110000 10000001 0....... ........ */
                /* la.decode:1076 */
                return LISA_VACCSADW_D_W;
            case 0x4:
                /* 01110000 10000010 0....... ........ */
                /* la.decode:1077 */
                return LISA_VACCSADW_H_BU;
            case 0x5:
                /* 01110000 10000010 1....... ........ */
                /* la.decode:1078 */
                return LISA_VACCSADW_W_HU;
            case 0x6:
                /* 01110000 10000011 0....... ........ */
                /* la.decode:1079 */
                return LISA_VACCSADW_D_WU;
            }
            return LISA_INVALID;
        case 0x21:
            /* 01110000 100001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10000100 0....... ........ */
                /* la.decode:1080 */
                return LISA_VMUL_B;
            case 0x1:
                /* 01110000 10000100 1....... ........ */
                /* la.decode:1081 */
                return LISA_VMUL_H;
            case 0x2:
                /* 01110000 10000101 0....... ........ */
                /* la.decode:1082 */
                return LISA_VMUL_W;
            case 0x3:
                /* 01110000 10000101 1....... ........ */
                /* la.decode:1083 */
                return LISA_VMUL_D;
            case 0x4:
                /* 01110000 10000110 0....... ........ */
                /* la.decode:1084 */
                return LISA_VMUH_B;
            case 0x5:
                /* 01110000 10000110 1....... ........ */
                /* la.decode:1085 */
                return LISA_VMUH_H;
            case 0x6:
                /* 01110000 10000111 0....... ........ */
                /* la.decode:1086 */
                return LISA_VMUH_W;
            case 0x7:
                /* 01110000 10000111 1....... ........ */
                /* la.decode:1087 */
                return LISA_VMUH_D;
            }
            return LISA_INVALID;
        case 0x22:
            /* 01110000 100010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10001000 0....... ........ */
                /* la.decode:1088 */
                return LISA_VMUH_BU;
            case 0x1:
                /* 01110000 10001000 1....... ........ */
                /* la.decode:1089 */
                return LISA_VMUH_HU;
            case 0x2:
                /* 01110000 10001001 0....... ........ */
                /* la.decode:1090 */
                return LISA_VMUH_WU;
            case 0x3:
                /* 01110000 10001001 1....... ........ */
                /* la.decode:1091 */
                return LISA_VMUH_DU;
            case 0x4:
                /* 01110000 10001010 0....... ........ */
                /* la.decode:1092 */
                return LISA_VMUH_BU_B;
            case 0x5:
                /* 01110000 10001010 1....... ........ */
                /* la.decode:1093 */
                return LISA_VMUH_HU_H;
            case 0x6:
                /* 01110000 10001011 0....... ........ */
                /* la.decode:1094 */
                return LISA_VMUH_WU_W;
            case 0x7:
                /* 01110000 10001011 1....... ........ */
                /* la.decode:1095 */
                return LISA_VMUH_DU_D;
            }
            return LISA_INVALID;
        case 0x23:
            /* 01110000 100011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10001100 0....... ........ */
                /* la.decode:1096 */
                return LISA_VMULXW_H_B;
            case 0x1:
                /* 01110000 10001100 1....... ........ */
                /* la.decode:1097 */
                return LISA_VMULXW_W_H;
            case 0x2:
                /* 01110000 10001101 0....... ........ */
                /* la.decode:1098 */
                return LISA_VMULXW_D_W;
            case 0x4:
                /* 01110000 10001110 0....... ........ */
                /* la.decode:1099 */
                return LISA_VMULXW_H_BU;
            case 0x5:
                /* 01110000 10001110 1....... ........ */
                /* la.decode:1100 */
                return LISA_VMULXW_W_HU;
            case 0x6:
                /* 01110000 10001111 0....... ........ */
                /* la.decode:1101 */
                return LISA_VMULXW_D_WU;
            }
            return LISA_INVALID;
        case 0x24:
            /* 01110000 100100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10010000 0....... ........ */
                /* la.decode:1102 */
                return LISA_VMULWEV_H_B;
            case 0x1:
                /* 01110000 10010000 1....... ........ */
                /* la.decode:1103 */
                return LISA_VMULWEV_W_H;
            case 0x2:
                /* 01110000 10010001 0....... ........ */
                /* la.decode:1104 */
                return LISA_VMULWEV_D_W;
            case 0x3:
                /* 01110000 10010001 1....... ........ */
                /* la.decode:1105 */
                return LISA_VMULWEV_Q_D;
            case 0x4:
                /* 01110000 10010010 0....... ........ */
                /* la.decode:1106 */
                return LISA_VMULWOD_H_B;
            case 0x5:
                /* 01110000 10010010 1....... ........ */
                /* la.decode:1107 */
                return LISA_VMULWOD_W_H;
            case 0x6:
                /* 01110000 10010011 0....... ........ */
                /* la.decode:1108 */
                return LISA_VMULWOD_D_W;
            case 0x7:
                /* 01110000 10010011 1....... ........ */
                /* la.decode:1109 */
                return LISA_VMULWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x25:
            /* 01110000 100101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10010100 0....... ........ */
                /* la.decode:1110 */
                return LISA_VMULWL_H_B;
            case 0x1:
                /* 01110000 10010100 1....... ........ */
                /* la.decode:1111 */
                return LISA_VMULWL_W_H;
            case 0x2:
                /* 01110000 10010101 0....... ........ */
                /* la.decode:1112 */
                return LISA_VMULWL_D_W;
            case 0x3:
                /* 01110000 10010101 1....... ........ */
                /* la.decode:1113 */
                return LISA_VMULWL_Q_D;
            case 0x4:
                /* 01110000 10010110 0....... ........ */
                /* la.decode:1114 */
                return LISA_VMULWH_H_B;
            case 0x5:
                /* 01110000 10010110 1....... ........ */
                /* la.decode:1115 */
                return LISA_VMULWH_W_H;
            case 0x6:
                /* 01110000 10010111 0....... ........ */
                /* la.decode:1116 */
                return LISA_VMULWH_D_W;
            case 0x7:
                /* 01110000 10010111 1....... ........ */
                /* la.decode:1117 */
                return LISA_VMULWH_Q_D;
            }
            return LISA_INVALID;
        case 0x26:
            /* 01110000 100110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10011000 0....... ........ */
                /* la.decode:1118 */
                return LISA_VMULWEV_H_BU;
            case 0x1:
                /* 01110000 10011000 1....... ........ */
                /* la.decode:1119 */
                return LISA_VMULWEV_W_HU;
            case 0x2:
                /* 01110000 10011001 0....... ........ */
                /* la.decode:1120 */
                return LISA_VMULWEV_D_WU;
            case 0x3:
                /* 01110000 10011001 1....... ........ */
                /* la.decode:1121 */
                return LISA_VMULWEV_Q_DU;
            case 0x4:
                /* 01110000 10011010 0....... ........ */
                /* la.decode:1122 */
                return LISA_VMULWOD_H_BU;
            case 0x5:
                /* 01110000 10011010 1....... ........ */
                /* la.decode:1123 */
                return LISA_VMULWOD_W_HU;
            case 0x6:
                /* 01110000 10011011 0....... ........ */
                /* la.decode:1124 */
                return LISA_VMULWOD_D_WU;
            case 0x7:
                /* 01110000 10011011 1....... ........ */
                /* la.decode:1125 */
                return LISA_VMULWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0x27:
            /* 01110000 100111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10011100 0....... ........ */
                /* la.decode:1126 */
                return LISA_VMULWL_H_BU;
            case 0x1:
                /* 01110000 10011100 1....... ........ */
                /* la.decode:1127 */
                return LISA_VMULWL_W_HU;
            case 0x2:
                /* 01110000 10011101 0....... ........ */
                /* la.decode:1128 */
                return LISA_VMULWL_D_WU;
            case 0x3:
                /* 01110000 10011101 1....... ........ */
                /* la.decode:1129 */
                return LISA_VMULWL_Q_DU;
            case 0x4:
                /* 01110000 10011110 0....... ........ */
                /* la.decode:1130 */
                return LISA_VMULWH_H_BU;
            case 0x5:
                /* 01110000 10011110 1....... ........ */
                /* la.decode:1131 */
                return LISA_VMULWH_W_HU;
            case 0x6:
                /* 01110000 10011111 0....... ........ */
                /* la.decode:1132 */
                return LISA_VMULWH_D_WU;
            case 0x7:
                /* 01110000 10011111 1....... ........ */
                /* la.decode:1133 */
                return LISA_VMULWH_Q_DU;
            }
            return LISA_INVALID;
        case 0x28:
            /* 01110000 101000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10100000 0....... ........ */
                /* la.decode:1134 */
                return LISA_VMULWEV_H_BU_B;
            case 0x1:
                /* 01110000 10100000 1....... ........ */
                /* la.decode:1135 */
                return LISA_VMULWEV_W_HU_H;
            case 0x2:
                /* 01110000 10100001 0....... ........ */
                /* la.decode:1136 */
                return LISA_VMULWEV_D_WU_W;
            case 0x3:
                /* 01110000 10100001 1....... ........ */
                /* la.decode:1137 */
                return LISA_VMULWEV_Q_DU_D;
            case 0x4:
                /* 01110000 10100010 0....... ........ */
                /* la.decode:1138 */
                return LISA_VMULWOD_H_BU_B;
            case 0x5:
                /* 01110000 10100010 1....... ........ */
                /* la.decode:1139 */
                return LISA_VMULWOD_W_HU_H;
            case 0x6:
                /* 01110000 10100011 0....... ........ */
                /* la.decode:1140 */
                return LISA_VMULWOD_D_WU_W;
            case 0x7:
                /* 01110000 10100011 1....... ........ */
                /* la.decode:1141 */
                return LISA_VMULWOD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x29:
            /* 01110000 101001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10100100 0....... ........ */
                /* la.decode:1142 */
                return LISA_VMULWL_H_BU_B;
            case 0x1:
                /* 01110000 10100100 1....... ........ */
                /* la.decode:1143 */
                return LISA_VMULWL_W_HU_H;
            case 0x2:
                /* 01110000 10100101 0....... ........ */
                /* la.decode:1144 */
                return LISA_VMULWL_D_WU_W;
            case 0x3:
                /* 01110000 10100101 1....... ........ */
                /* la.decode:1145 */
                return LISA_VMULWL_Q_DU_D;
            case 0x4:
                /* 01110000 10100110 0....... ........ */
                /* la.decode:1146 */
                return LISA_VMULWH_H_BU_B;
            case 0x5:
                /* 01110000 10100110 1....... ........ */
                /* la.decode:1147 */
                return LISA_VMULWH_W_HU_H;
            case 0x6:
                /* 01110000 10100111 0....... ........ */
                /* la.decode:1148 */
                return LISA_VMULWH_D_WU_W;
            case 0x7:
                /* 01110000 10100111 1....... ........ */
                /* la.decode:1149 */
                return LISA_VMULWH_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x2a:
            /* 01110000 101010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10101000 0....... ........ */
                /* la.decode:1150 */
                return LISA_VMADD_B;
            case 0x1:
                /* 01110000 10101000 1....... ........ */
                /* la.decode:1151 */
                return LISA_VMADD_H;
            case 0x2:
                /* 01110000 10101001 0....... ........ */
                /* la.decode:1152 */
                return LISA_VMADD_W;
            case 0x3:
                /* 01110000 10101001 1....... ........ */
                /* la.decode:1153 */
                return LISA_VMADD_D;
            case 0x4:
                /* 01110000 10101010 0....... ........ */
                /* la.decode:1154 */
                return LISA_VMSUB_B;
            case 0x5:
                /* 01110000 10101010 1....... ........ */
                /* la.decode:1155 */
                return LISA_VMSUB_H;
            case 0x6:
                /* 01110000 10101011 0....... ........ */
                /* la.decode:1156 */
                return LISA_VMSUB_W;
            case 0x7:
                /* 01110000 10101011 1....... ........ */
                /* la.decode:1157 */
                return LISA_VMSUB_D;
            }
            return LISA_INVALID;
        case 0x2b:
            /* 01110000 101011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10101100 0....... ........ */
                /* la.decode:1158 */
                return LISA_VMADDWEV_H_B;
            case 0x1:
                /* 01110000 10101100 1....... ........ */
                /* la.decode:1159 */
                return LISA_VMADDWEV_W_H;
            case 0x2:
                /* 01110000 10101101 0....... ........ */
                /* la.decode:1160 */
                return LISA_VMADDWEV_D_W;
            case 0x3:
                /* 01110000 10101101 1....... ........ */
                /* la.decode:1161 */
                return LISA_VMADDWEV_Q_D;
            case 0x4:
                /* 01110000 10101110 0....... ........ */
                /* la.decode:1162 */
                return LISA_VMADDWOD_H_B;
            case 0x5:
                /* 01110000 10101110 1....... ........ */
                /* la.decode:1163 */
                return LISA_VMADDWOD_W_H;
            case 0x6:
                /* 01110000 10101111 0....... ........ */
                /* la.decode:1164 */
                return LISA_VMADDWOD_D_W;
            case 0x7:
                /* 01110000 10101111 1....... ........ */
                /* la.decode:1165 */
                return LISA_VMADDWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x2c:
            /* 01110000 101100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10110000 0....... ........ */
                /* la.decode:1166 */
                return LISA_VMADDWL_H_B;
            case 0x1:
                /* 01110000 10110000 1....... ........ */
                /* la.decode:1167 */
                return LISA_VMADDWL_W_H;
            case 0x2:
                /* 01110000 10110001 0....... ........ */
                /* la.decode:1168 */
                return LISA_VMADDWL_D_W;
            case 0x3:
                /* 01110000 10110001 1....... ........ */
                /* la.decode:1169 */
                return LISA_VMADDWL_Q_D;
            case 0x4:
                /* 01110000 10110010 0....... ........ */
                /* la.decode:1170 */
                return LISA_VMADDWH_H_B;
            case 0x5:
                /* 01110000 10110010 1....... ........ */
                /* la.decode:1171 */
                return LISA_VMADDWH_W_H;
            case 0x6:
                /* 01110000 10110011 0....... ........ */
                /* la.decode:1172 */
                return LISA_VMADDWH_D_W;
            case 0x7:
                /* 01110000 10110011 1....... ........ */
                /* la.decode:1173 */
                return LISA_VMADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0x2d:
            /* 01110000 101101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10110100 0....... ........ */
                /* la.decode:1174 */
                return LISA_VMADDWEV_H_BU;
            case 0x1:
                /* 01110000 10110100 1....... ........ */
                /* la.decode:1175 */
                return LISA_VMADDWEV_W_HU;
            case 0x2:
                /* 01110000 10110101 0....... ........ */
                /* la.decode:1176 */
                return LISA_VMADDWEV_D_WU;
            case 0x3:
                /* 01110000 10110101 1....... ........ */
                /* la.decode:1177 */
                return LISA_VMADDWEV_Q_DU;
            case 0x4:
                /* 01110000 10110110 0....... ........ */
                /* la.decode:1178 */
                return LISA_VMADDWOD_H_BU;
            case 0x5:
                /* 01110000 10110110 1....... ........ */
                /* la.decode:1179 */
                return LISA_VMADDWOD_W_HU;
            case 0x6:
                /* 01110000 10110111 0....... ........ */
                /* la.decode:1180 */
                return LISA_VMADDWOD_D_WU;
            case 0x7:
                /* 01110000 10110111 1....... ........ */
                /* la.decode:1181 */
                return LISA_VMADDWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0x2e:
            /* 01110000 101110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10111000 0....... ........ */
                /* la.decode:1182 */
                return LISA_VMADDWL_H_BU;
            case 0x1:
                /* 01110000 10111000 1....... ........ */
                /* la.decode:1183 */
                return LISA_VMADDWL_W_HU;
            case 0x2:
                /* 01110000 10111001 0....... ........ */
                /* la.decode:1184 */
                return LISA_VMADDWL_D_WU;
            case 0x3:
                /* 01110000 10111001 1....... ........ */
                /* la.decode:1185 */
                return LISA_VMADDWL_Q_DU;
            case 0x4:
                /* 01110000 10111010 0....... ........ */
                /* la.decode:1186 */
                return LISA_VMADDWH_H_BU;
            case 0x5:
                /* 01110000 10111010 1....... ........ */
                /* la.decode:1187 */
                return LISA_VMADDWH_W_HU;
            case 0x6:
                /* 01110000 10111011 0....... ........ */
                /* la.decode:1188 */
                return LISA_VMADDWH_D_WU;
            case 0x7:
                /* 01110000 10111011 1....... ........ */
                /* la.decode:1189 */
                return LISA_VMADDWH_Q_DU;
            }
            return LISA_INVALID;
        case 0x2f:
            /* 01110000 101111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 10111100 0....... ........ */
                /* la.decode:1190 */
                return LISA_VMADDWEV_H_BU_B;
            case 0x1:
                /* 01110000 10111100 1....... ........ */
                /* la.decode:1191 */
                return LISA_VMADDWEV_W_HU_H;
            case 0x2:
                /* 01110000 10111101 0....... ........ */
                /* la.decode:1192 */
                return LISA_VMADDWEV_D_WU_W;
            case 0x3:
                /* 01110000 10111101 1....... ........ */
                /* la.decode:1193 */
                return LISA_VMADDWEV_Q_DU_D;
            case 0x4:
                /* 01110000 10111110 0....... ........ */
                /* la.decode:1194 */
                return LISA_VMADDWOD_H_BU_B;
            case 0x5:
                /* 01110000 10111110 1....... ........ */
                /* la.decode:1195 */
                return LISA_VMADDWOD_W_HU_H;
            case 0x6:
                /* 01110000 10111111 0....... ........ */
                /* la.decode:1196 */
                return LISA_VMADDWOD_D_WU_W;
            case 0x7:
                /* 01110000 10111111 1....... ........ */
                /* la.decode:1197 */
                return LISA_VMADDWOD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x30:
            /* 01110000 110000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11000000 0....... ........ */
                /* la.decode:1198 */
                return LISA_VMADDWL_H_BU_B;
            case 0x1:
                /* 01110000 11000000 1....... ........ */
                /* la.decode:1199 */
                return LISA_VMADDWL_W_HU_H;
            case 0x2:
                /* 01110000 11000001 0....... ........ */
                /* la.decode:1200 */
                return LISA_VMADDWL_D_WU_W;
            case 0x3:
                /* 01110000 11000001 1....... ........ */
                /* la.decode:1201 */
                return LISA_VMADDWL_Q_DU_D;
            case 0x4:
                /* 01110000 11000010 0....... ........ */
                /* la.decode:1202 */
                return LISA_VMADDWH_H_BU_B;
            case 0x5:
                /* 01110000 11000010 1....... ........ */
                /* la.decode:1203 */
                return LISA_VMADDWH_W_HU_H;
            case 0x6:
                /* 01110000 11000011 0....... ........ */
                /* la.decode:1204 */
                return LISA_VMADDWH_D_WU_W;
            case 0x7:
                /* 01110000 11000011 1....... ........ */
                /* la.decode:1205 */
                return LISA_VMADDWH_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x31:
            /* 01110000 110001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11000100 0....... ........ */
                /* la.decode:1206 */
                return LISA_VDP2_H_B;
            case 0x1:
                /* 01110000 11000100 1....... ........ */
                /* la.decode:1207 */
                return LISA_VDP2_W_H;
            case 0x2:
                /* 01110000 11000101 0....... ........ */
                /* la.decode:1208 */
                return LISA_VDP2_D_W;
            case 0x3:
                /* 01110000 11000101 1....... ........ */
                /* la.decode:1209 */
                return LISA_VDP2_Q_D;
            case 0x4:
                /* 01110000 11000110 0....... ........ */
                /* la.decode:1210 */
                return LISA_VDP2_HU_BU;
            case 0x5:
                /* 01110000 11000110 1....... ........ */
                /* la.decode:1211 */
                return LISA_VDP2_WU_HU;
            case 0x6:
                /* 01110000 11000111 0....... ........ */
                /* la.decode:1212 */
                return LISA_VDP2_DU_WU;
            case 0x7:
                /* 01110000 11000111 1....... ........ */
                /* la.decode:1213 */
                return LISA_VDP2_QU_DU;
            }
            return LISA_INVALID;
        case 0x32:
            /* 01110000 110010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11001000 0....... ........ */
                /* la.decode:1214 */
                return LISA_VDP2_H_BU_B;
            case 0x1:
                /* 01110000 11001000 1....... ........ */
                /* la.decode:1215 */
                return LISA_VDP2_W_HU_H;
            case 0x2:
                /* 01110000 11001001 0....... ........ */
                /* la.decode:1216 */
                return LISA_VDP2_D_WU_W;
            case 0x3:
                /* 01110000 11001001 1....... ........ */
                /* la.decode:1217 */
                return LISA_VDP2_Q_DU_D;
            case 0x4:
                /* 01110000 11001010 0....... ........ */
                /* la.decode:1218 */
                return LISA_VDP2ADD_H_B;
            case 0x5:
                /* 01110000 11001010 1....... ........ */
                /* la.decode:1219 */
                return LISA_VDP2ADD_W_H;
            case 0x6:
                /* 01110000 11001011 0....... ........ */
                /* la.decode:1220 */
                return LISA_VDP2ADD_D_W;
            case 0x7:
                /* 01110000 11001011 1....... ........ */
                /* la.decode:1221 */
                return LISA_VDP2ADD_Q_D;
            }
            return LISA_INVALID;
        case 0x33:
            /* 01110000 110011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11001100 0....... ........ */
                /* la.decode:1222 */
                return LISA_VDP2ADD_H_BU;
            case 0x1:
                /* 01110000 11001100 1....... ........ */
                /* la.decode:1223 */
                return LISA_VDP2ADD_W_HU;
            case 0x2:
                /* 01110000 11001101 0....... ........ */
                /* la.decode:1224 */
                return LISA_VDP2ADD_D_WU;
            case 0x3:
                /* 01110000 11001101 1....... ........ */
                /* la.decode:1225 */
                return LISA_VDP2ADD_Q_DU;
            case 0x4:
                /* 01110000 11001110 0....... ........ */
                /* la.decode:1226 */
                return LISA_VDP2ADD_H_BU_B;
            case 0x5:
                /* 01110000 11001110 1....... ........ */
                /* la.decode:1227 */
                return LISA_VDP2ADD_W_HU_H;
            case 0x6:
                /* 01110000 11001111 0....... ........ */
                /* la.decode:1228 */
                return LISA_VDP2ADD_D_WU_W;
            case 0x7:
                /* 01110000 11001111 1....... ........ */
                /* la.decode:1229 */
                return LISA_VDP2ADD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x34:
            /* 01110000 110100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11010000 0....... ........ */
                /* la.decode:1230 */
                return LISA_VDP2SUB_H_B;
            case 0x1:
                /* 01110000 11010000 1....... ........ */
                /* la.decode:1231 */
                return LISA_VDP2SUB_W_H;
            case 0x2:
                /* 01110000 11010001 0....... ........ */
                /* la.decode:1232 */
                return LISA_VDP2SUB_D_W;
            case 0x3:
                /* 01110000 11010001 1....... ........ */
                /* la.decode:1233 */
                return LISA_VDP2SUB_Q_D;
            case 0x4:
                /* 01110000 11010010 0....... ........ */
                /* la.decode:1234 */
                return LISA_VDP2SUB_H_BU;
            case 0x5:
                /* 01110000 11010010 1....... ........ */
                /* la.decode:1235 */
                return LISA_VDP2SUB_W_HU;
            case 0x6:
                /* 01110000 11010011 0....... ........ */
                /* la.decode:1236 */
                return LISA_VDP2SUB_D_WU;
            case 0x7:
                /* 01110000 11010011 1....... ........ */
                /* la.decode:1237 */
                return LISA_VDP2SUB_Q_DU;
            }
            return LISA_INVALID;
        case 0x35:
            /* 01110000 110101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11010100 0....... ........ */
                /* la.decode:1238 */
                return LISA_VDP4_W_B;
            case 0x1:
                /* 01110000 11010100 1....... ........ */
                /* la.decode:1239 */
                return LISA_VDP4_D_H;
            case 0x2:
                /* 01110000 11010101 0....... ........ */
                /* la.decode:1240 */
                return LISA_VDP4_Q_W;
            case 0x4:
                /* 01110000 11010110 0....... ........ */
                /* la.decode:1241 */
                return LISA_VDP4_W_BU;
            case 0x5:
                /* 01110000 11010110 1....... ........ */
                /* la.decode:1242 */
                return LISA_VDP4_D_HU;
            case 0x6:
                /* 01110000 11010111 0....... ........ */
                /* la.decode:1243 */
                return LISA_VDP4_Q_WU;
            }
            return LISA_INVALID;
        case 0x36:
            /* 01110000 110110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11011000 0....... ........ */
                /* la.decode:1244 */
                return LISA_VDP4_W_BU_B;
            case 0x1:
                /* 01110000 11011000 1....... ........ */
                /* la.decode:1245 */
                return LISA_VDP4_D_HU_H;
            case 0x2:
                /* 01110000 11011001 0....... ........ */
                /* la.decode:1246 */
                return LISA_VDP4_Q_WU_W;
            case 0x4:
                /* 01110000 11011010 0....... ........ */
                /* la.decode:1247 */
                return LISA_VDP4ADD_W_B;
            case 0x5:
                /* 01110000 11011010 1....... ........ */
                /* la.decode:1248 */
                return LISA_VDP4ADD_D_H;
            case 0x6:
                /* 01110000 11011011 0....... ........ */
                /* la.decode:1249 */
                return LISA_VDP4ADD_Q_W;
            }
            return LISA_INVALID;
        case 0x37:
            /* 01110000 110111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11011100 0....... ........ */
                /* la.decode:1250 */
                return LISA_VDP4ADD_W_BU;
            case 0x1:
                /* 01110000 11011100 1....... ........ */
                /* la.decode:1251 */
                return LISA_VDP4ADD_D_HU;
            case 0x2:
                /* 01110000 11011101 0....... ........ */
                /* la.decode:1252 */
                return LISA_VDP4ADD_Q_WU;
            case 0x4:
                /* 01110000 11011110 0....... ........ */
                /* la.decode:1253 */
                return LISA_VDP4ADD_W_BU_B;
            case 0x5:
                /* 01110000 11011110 1....... ........ */
                /* la.decode:1254 */
                return LISA_VDP4ADD_D_HU_H;
            case 0x6:
                /* 01110000 11011111 0....... ........ */
                /* la.decode:1255 */
                return LISA_VDP4ADD_Q_WU_W;
            }
            return LISA_INVALID;
        case 0x38:
            /* 01110000 111000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11100000 0....... ........ */
                /* la.decode:1256 */
                return LISA_VDIV_B;
            case 0x1:
                /* 01110000 11100000 1....... ........ */
                /* la.decode:1257 */
                return LISA_VDIV_H;
            case 0x2:
                /* 01110000 11100001 0....... ........ */
                /* la.decode:1258 */
                return LISA_VDIV_W;
            case 0x3:
                /* 01110000 11100001 1....... ........ */
                /* la.decode:1259 */
                return LISA_VDIV_D;
            case 0x4:
                /* 01110000 11100010 0....... ........ */
                /* la.decode:1260 */
                return LISA_VMOD_B;
            case 0x5:
                /* 01110000 11100010 1....... ........ */
                /* la.decode:1261 */
                return LISA_VMOD_H;
            case 0x6:
                /* 01110000 11100011 0....... ........ */
                /* la.decode:1262 */
                return LISA_VMOD_W;
            case 0x7:
                /* 01110000 11100011 1....... ........ */
                /* la.decode:1263 */
                return LISA_VMOD_D;
            }
            return LISA_INVALID;
        case 0x39:
            /* 01110000 111001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11100100 0....... ........ */
                /* la.decode:1264 */
                return LISA_VDIV_BU;
            case 0x1:
                /* 01110000 11100100 1....... ........ */
                /* la.decode:1265 */
                return LISA_VDIV_HU;
            case 0x2:
                /* 01110000 11100101 0....... ........ */
                /* la.decode:1266 */
                return LISA_VDIV_WU;
            case 0x3:
                /* 01110000 11100101 1....... ........ */
                /* la.decode:1267 */
                return LISA_VDIV_DU;
            case 0x4:
                /* 01110000 11100110 0....... ........ */
                /* la.decode:1268 */
                return LISA_VMOD_BU;
            case 0x5:
                /* 01110000 11100110 1....... ........ */
                /* la.decode:1269 */
                return LISA_VMOD_HU;
            case 0x6:
                /* 01110000 11100111 0....... ........ */
                /* la.decode:1270 */
                return LISA_VMOD_WU;
            case 0x7:
                /* 01110000 11100111 1....... ........ */
                /* la.decode:1271 */
                return LISA_VMOD_DU;
            }
            return LISA_INVALID;
        case 0x3a:
            /* 01110000 111010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11101000 0....... ........ */
                /* la.decode:1272 */
                return LISA_VSLL_B;
            case 0x1:
                /* 01110000 11101000 1....... ........ */
                /* la.decode:1273 */
                return LISA_VSLL_H;
            case 0x2:
                /* 01110000 11101001 0....... ........ */
                /* la.decode:1274 */
                return LISA_VSLL_W;
            case 0x3:
                /* 01110000 11101001 1....... ........ */
                /* la.decode:1275 */
                return LISA_VSLL_D;
            case 0x4:
                /* 01110000 11101010 0....... ........ */
                /* la.decode:1276 */
                return LISA_VSRL_B;
            case 0x5:
                /* 01110000 11101010 1....... ........ */
                /* la.decode:1277 */
                return LISA_VSRL_H;
            case 0x6:
                /* 01110000 11101011 0....... ........ */
                /* la.decode:1278 */
                return LISA_VSRL_W;
            case 0x7:
                /* 01110000 11101011 1....... ........ */
                /* la.decode:1279 */
                return LISA_VSRL_D;
            }
            return LISA_INVALID;
        case 0x3b:
            /* 01110000 111011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11101100 0....... ........ */
                /* la.decode:1280 */
                return LISA_VSRA_B;
            case 0x1:
                /* 01110000 11101100 1....... ........ */
                /* la.decode:1281 */
                return LISA_VSRA_H;
            case 0x2:
                /* 01110000 11101101 0....... ........ */
                /* la.decode:1282 */
                return LISA_VSRA_W;
            case 0x3:
                /* 01110000 11101101 1....... ........ */
                /* la.decode:1283 */
                return LISA_VSRA_D;
            case 0x4:
                /* 01110000 11101110 0....... ........ */
                /* la.decode:1284 */
                return LISA_VROTR_B;
            case 0x5:
                /* 01110000 11101110 1....... ........ */
                /* la.decode:1285 */
                return LISA_VROTR_H;
            case 0x6:
                /* 01110000 11101111 0....... ........ */
                /* la.decode:1286 */
                return LISA_VROTR_W;
            case 0x7:
                /* 01110000 11101111 1....... ........ */
                /* la.decode:1287 */
                return LISA_VROTR_D;
            }
            return LISA_INVALID;
        case 0x3c:
            /* 01110000 111100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110000 11110000 0....... ........ */
                /* la.decode:1288 */
                return LISA_VSRLR_B;
            case 0x1:
                /* 01110000 11110000 1....... ........ */
                /* la.decode:1289 */
                return LISA_VSRLR_H;
            case 0x2:
                /* 01110000 11110001 0....... ........ */
                /* la.decode:1290 */
                return LISA_VSRLR_W;
            case 0x3:
                /* 01110000 11110001 1....... ........ */
                /* la.decode:1291 */
                return LISA_VSRLR_D;
            case 0x4:
                /* 01110000 11110010 0....... ........ */
                /* la.decode:1292 */
                return LISA_VSRAR_B;
            case 0x5:
                /* 01110000 11110010 1....... ........ */
                /* la.decode:1293 */
                return LISA_VSRAR_H;
            case 0x6:
                /* 01110000 11110011 0....... ........ */
                /* la.decode:1294 */
                return LISA_VSRAR_W;
            case 0x7:
                /* 01110000 11110011 1....... ........ */
                /* la.decode:1295 */
                return LISA_VSRAR_D;
            }
            return LISA_INVALID;
        case 0x3d:
            /* 01110000 111101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110000 11110100 1....... ........ */
                /* la.decode:1296 */
                return LISA_VSRLN_B_H;
            case 0x2:
                /* 01110000 11110101 0....... ........ */
                /* la.decode:1297 */
                return LISA_VSRLN_H_W;
            case 0x3:
                /* 01110000 11110101 1....... ........ */
                /* la.decode:1298 */
                return LISA_VSRLN_W_D;
            case 0x5:
                /* 01110000 11110110 1....... ........ */
                /* la.decode:1299 */
                return LISA_VSRAN_B_H;
            case 0x6:
                /* 01110000 11110111 0....... ........ */
                /* la.decode:1300 */
                return LISA_VSRAN_H_W;
            case 0x7:
                /* 01110000 11110111 1....... ........ */
                /* la.decode:1301 */
                return LISA_VSRAN_W_D;
            }
            return LISA_INVALID;
        case 0x3e:
            /* 01110000 111110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110000 11111000 1....... ........ */
                /* la.decode:1302 */
                return LISA_VSRLRN_B_H;
            case 0x2:
                /* 01110000 11111001 0....... ........ */
                /* la.decode:1303 */
                return LISA_VSRLRN_H_W;
            case 0x3:
                /* 01110000 11111001 1....... ........ */
                /* la.decode:1304 */
                return LISA_VSRLRN_W_D;
            case 0x5:
                /* 01110000 11111010 1....... ........ */
                /* la.decode:1305 */
                return LISA_VSRARN_B_H;
            case 0x6:
                /* 01110000 11111011 0....... ........ */
                /* la.decode:1306 */
                return LISA_VSRARN_H_W;
            case 0x7:
                /* 01110000 11111011 1....... ........ */
                /* la.decode:1307 */
                return LISA_VSRARN_W_D;
            }
            return LISA_INVALID;
        case 0x3f:
            /* 01110000 111111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110000 11111100 1....... ........ */
                /* la.decode:1308 */
                return LISA_VSSRLN_B_H;
            case 0x2:
                /* 01110000 11111101 0....... ........ */
                /* la.decode:1309 */
                return LISA_VSSRLN_H_W;
            case 0x3:
                /* 01110000 11111101 1....... ........ */
                /* la.decode:1310 */
                return LISA_VSSRLN_W_D;
            case 0x5:
                /* 01110000 11111110 1....... ........ */
                /* la.decode:1311 */
                return LISA_VSSRAN_B_H;
            case 0x6:
                /* 01110000 11111111 0....... ........ */
                /* la.decode:1312 */
                return LISA_VSSRAN_H_W;
            case 0x7:
                /* 01110000 11111111 1....... ........ */
                /* la.decode:1313 */
                return LISA_VSSRAN_W_D;
            }
            return LISA_INVALID;
        case 0x40:
            /* 01110001 000000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110001 00000000 1....... ........ */
                /* la.decode:1314 */
                return LISA_VSSRLRN_B_H;
            case 0x2:
                /* 01110001 00000001 0....... ........ */
                /* la.decode:1315 */
                return LISA_VSSRLRN_H_W;
            case 0x3:
                /* 01110001 00000001 1....... ........ */
                /* la.decode:1316 */
                return LISA_VSSRLRN_W_D;
            case 0x5:
                /* 01110001 00000010 1....... ........ */
                /* la.decode:1317 */
                return LISA_VSSRARN_B_H;
            case 0x6:
                /* 01110001 00000011 0....... ........ */
                /* la.decode:1318 */
                return LISA_VSSRARN_H_W;
            case 0x7:
                /* 01110001 00000011 1....... ........ */
                /* la.decode:1319 */
                return LISA_VSSRARN_W_D;
            }
            return LISA_INVALID;
        case 0x41:
            /* 01110001 000001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110001 00000100 1....... ........ */
                /* la.decode:1320 */
                return LISA_VSSRLN_BU_H;
            case 0x2:
                /* 01110001 00000101 0....... ........ */
                /* la.decode:1321 */
                return LISA_VSSRLN_HU_W;
            case 0x3:
                /* 01110001 00000101 1....... ........ */
                /* la.decode:1322 */
                return LISA_VSSRLN_WU_D;
            case 0x5:
                /* 01110001 00000110 1....... ........ */
                /* la.decode:1323 */
                return LISA_VSSRAN_BU_H;
            case 0x6:
                /* 01110001 00000111 0....... ........ */
                /* la.decode:1324 */
                return LISA_VSSRAN_HU_W;
            case 0x7:
                /* 01110001 00000111 1....... ........ */
                /* la.decode:1325 */
                return LISA_VSSRAN_WU_D;
            }
            return LISA_INVALID;
        case 0x42:
            /* 01110001 000010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110001 00001000 1....... ........ */
                /* la.decode:1326 */
                return LISA_VSSRLRN_BU_H;
            case 0x2:
                /* 01110001 00001001 0....... ........ */
                /* la.decode:1327 */
                return LISA_VSSRLRN_HU_W;
            case 0x3:
                /* 01110001 00001001 1....... ........ */
                /* la.decode:1328 */
                return LISA_VSSRLRN_WU_D;
            case 0x5:
                /* 01110001 00001010 1....... ........ */
                /* la.decode:1329 */
                return LISA_VSSRARN_BU_H;
            case 0x6:
                /* 01110001 00001011 0....... ........ */
                /* la.decode:1330 */
                return LISA_VSSRARN_HU_W;
            case 0x7:
                /* 01110001 00001011 1....... ........ */
                /* la.decode:1331 */
                return LISA_VSSRARN_WU_D;
            }
            return LISA_INVALID;
        case 0x43:
            /* 01110001 000011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00001100 0....... ........ */
                /* la.decode:1332 */
                return LISA_VBITCLR_B;
            case 0x1:
                /* 01110001 00001100 1....... ........ */
                /* la.decode:1333 */
                return LISA_VBITCLR_H;
            case 0x2:
                /* 01110001 00001101 0....... ........ */
                /* la.decode:1334 */
                return LISA_VBITCLR_W;
            case 0x3:
                /* 01110001 00001101 1....... ........ */
                /* la.decode:1335 */
                return LISA_VBITCLR_D;
            case 0x4:
                /* 01110001 00001110 0....... ........ */
                /* la.decode:1336 */
                return LISA_VBITSET_B;
            case 0x5:
                /* 01110001 00001110 1....... ........ */
                /* la.decode:1337 */
                return LISA_VBITSET_H;
            case 0x6:
                /* 01110001 00001111 0....... ........ */
                /* la.decode:1338 */
                return LISA_VBITSET_W;
            case 0x7:
                /* 01110001 00001111 1....... ........ */
                /* la.decode:1339 */
                return LISA_VBITSET_D;
            }
            return LISA_INVALID;
        case 0x44:
            /* 01110001 000100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00010000 0....... ........ */
                /* la.decode:1340 */
                return LISA_VBITREV_B;
            case 0x1:
                /* 01110001 00010000 1....... ........ */
                /* la.decode:1341 */
                return LISA_VBITREV_H;
            case 0x2:
                /* 01110001 00010001 0....... ........ */
                /* la.decode:1342 */
                return LISA_VBITREV_W;
            case 0x3:
                /* 01110001 00010001 1....... ........ */
                /* la.decode:1343 */
                return LISA_VBITREV_D;
            case 0x4:
                /* 01110001 00010010 0....... ........ */
                /* la.decode:1344 */
                return LISA_VBSTRC12_B;
            case 0x5:
                /* 01110001 00010010 1....... ........ */
                /* la.decode:1345 */
                return LISA_VBSTRC12_H;
            case 0x6:
                /* 01110001 00010011 0....... ........ */
                /* la.decode:1346 */
                return LISA_VBSTRC12_W;
            case 0x7:
                /* 01110001 00010011 1....... ........ */
                /* la.decode:1347 */
                return LISA_VBSTRC12_D;
            }
            return LISA_INVALID;
        case 0x45:
            /* 01110001 000101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00010100 0....... ........ */
                /* la.decode:1348 */
                return LISA_VBSTRC21_B;
            case 0x1:
                /* 01110001 00010100 1....... ........ */
                /* la.decode:1349 */
                return LISA_VBSTRC21_H;
            case 0x2:
                /* 01110001 00010101 0....... ........ */
                /* la.decode:1350 */
                return LISA_VBSTRC21_W;
            case 0x3:
                /* 01110001 00010101 1....... ........ */
                /* la.decode:1351 */
                return LISA_VBSTRC21_D;
            case 0x4:
                /* 01110001 00010110 0....... ........ */
                /* la.decode:1352 */
                return LISA_VPACKEV_B;
            case 0x5:
                /* 01110001 00010110 1....... ........ */
                /* la.decode:1353 */
                return LISA_VPACKEV_H;
            case 0x6:
                /* 01110001 00010111 0....... ........ */
                /* la.decode:1354 */
                return LISA_VPACKEV_W;
            case 0x7:
                /* 01110001 00010111 1....... ........ */
                /* la.decode:1355 */
                return LISA_VPACKEV_D;
            }
            return LISA_INVALID;
        case 0x46:
            /* 01110001 000110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00011000 0....... ........ */
                /* la.decode:1356 */
                return LISA_VPACKOD_B;
            case 0x1:
                /* 01110001 00011000 1....... ........ */
                /* la.decode:1357 */
                return LISA_VPACKOD_H;
            case 0x2:
                /* 01110001 00011001 0....... ........ */
                /* la.decode:1358 */
                return LISA_VPACKOD_W;
            case 0x3:
                /* 01110001 00011001 1....... ........ */
                /* la.decode:1359 */
                return LISA_VPACKOD_D;
            case 0x4:
                /* 01110001 00011010 0....... ........ */
                /* la.decode:1360 */
                return LISA_VILVL_B;
            case 0x5:
                /* 01110001 00011010 1....... ........ */
                /* la.decode:1361 */
                return LISA_VILVL_H;
            case 0x6:
                /* 01110001 00011011 0....... ........ */
                /* la.decode:1362 */
                return LISA_VILVL_W;
            case 0x7:
                /* 01110001 00011011 1....... ........ */
                /* la.decode:1363 */
                return LISA_VILVL_D;
            }
            return LISA_INVALID;
        case 0x47:
            /* 01110001 000111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00011100 0....... ........ */
                /* la.decode:1364 */
                return LISA_VILVH_B;
            case 0x1:
                /* 01110001 00011100 1....... ........ */
                /* la.decode:1365 */
                return LISA_VILVH_H;
            case 0x2:
                /* 01110001 00011101 0....... ........ */
                /* la.decode:1366 */
                return LISA_VILVH_W;
            case 0x3:
                /* 01110001 00011101 1....... ........ */
                /* la.decode:1367 */
                return LISA_VILVH_D;
            case 0x4:
                /* 01110001 00011110 0....... ........ */
                /* la.decode:1368 */
                return LISA_VPICKEV_B;
            case 0x5:
                /* 01110001 00011110 1....... ........ */
                /* la.decode:1369 */
                return LISA_VPICKEV_H;
            case 0x6:
                /* 01110001 00011111 0....... ........ */
                /* la.decode:1370 */
                return LISA_VPICKEV_W;
            case 0x7:
                /* 01110001 00011111 1....... ........ */
                /* la.decode:1371 */
                return LISA_VPICKEV_D;
            }
            return LISA_INVALID;
        case 0x48:
            /* 01110001 001000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00100000 0....... ........ */
                /* la.decode:1372 */
                return LISA_VPICKOD_B;
            case 0x1:
                /* 01110001 00100000 1....... ........ */
                /* la.decode:1373 */
                return LISA_VPICKOD_H;
            case 0x2:
                /* 01110001 00100001 0....... ........ */
                /* la.decode:1374 */
                return LISA_VPICKOD_W;
            case 0x3:
                /* 01110001 00100001 1....... ........ */
                /* la.decode:1375 */
                return LISA_VPICKOD_D;
            case 0x4:
                /* 01110001 00100010 0....... ........ */
                /* la.decode:1376 */
                return LISA_VREPLVE_B;
            case 0x5:
                /* 01110001 00100010 1....... ........ */
                /* la.decode:1377 */
                return LISA_VREPLVE_H;
            case 0x6:
                /* 01110001 00100011 0....... ........ */
                /* la.decode:1378 */
                return LISA_VREPLVE_W;
            case 0x7:
                /* 01110001 00100011 1....... ........ */
                /* la.decode:1379 */
                return LISA_VREPLVE_D;
            }
            return LISA_INVALID;
        case 0x49:
            /* 01110001 001001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00100100 0....... ........ */
                /* la.decode:1380 */
                return LISA_VEXTRCOL_B;
            case 0x1:
                /* 01110001 00100100 1....... ........ */
                /* la.decode:1381 */
                return LISA_VEXTRCOL_H;
            case 0x2:
                /* 01110001 00100101 0....... ........ */
                /* la.decode:1382 */
                return LISA_VEXTRCOL_W;
            case 0x3:
                /* 01110001 00100101 1....... ........ */
                /* la.decode:1383 */
                return LISA_VEXTRCOL_D;
            case 0x4:
                /* 01110001 00100110 0....... ........ */
                /* la.decode:1384 */
                return LISA_VAND_V;
            case 0x5:
                /* 01110001 00100110 1....... ........ */
                /* la.decode:1385 */
                return LISA_VOR_V;
            case 0x6:
                /* 01110001 00100111 0....... ........ */
                /* la.decode:1386 */
                return LISA_VXOR_V;
            case 0x7:
                /* 01110001 00100111 1....... ........ */
                /* la.decode:1387 */
                return LISA_VNOR_V;
            }
            return LISA_INVALID;
        case 0x4a:
            /* 01110001 001010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00101000 0....... ........ */
                /* la.decode:1388 */
                return LISA_VANDN_V;
            case 0x1:
                /* 01110001 00101000 1....... ........ */
                /* la.decode:1389 */
                return LISA_VORN_V;
            case 0x2:
                /* 01110001 00101001 0....... ........ */
                /* la.decode:1390 */
                return LISA_VRANDSIGN_B;
            case 0x3:
                /* 01110001 00101001 1....... ........ */
                /* la.decode:1391 */
                return LISA_VRANDSIGN_H;
            case 0x4:
                /* 01110001 00101010 0....... ........ */
                /* la.decode:1392 */
                return LISA_VRORSIGN_B;
            case 0x5:
                /* 01110001 00101010 1....... ........ */
                /* la.decode:1393 */
                return LISA_VRORSIGN_H;
            case 0x6:
                /* 01110001 00101011 0....... ........ */
                /* la.decode:1394 */
                return LISA_VFRSTP_B;
            case 0x7:
                /* 01110001 00101011 1....... ........ */
                /* la.decode:1395 */
                return LISA_VFRSTP_H;
            }
            return LISA_INVALID;
        case 0x4b:
            /* 01110001 001011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00101100 0....... ........ */
                /* la.decode:1396 */
                return LISA_VCLRSTRR_V;
            case 0x1:
                /* 01110001 00101100 1....... ........ */
                /* la.decode:1397 */
                return LISA_VCLRSTRV_V;
            case 0x2:
                /* 01110001 00101101 0....... ........ */
                /* la.decode:1398 */
                return LISA_VADD_Q;
            case 0x3:
                /* 01110001 00101101 1....... ........ */
                /* la.decode:1399 */
                return LISA_VSUB_Q;
            case 0x4:
                /* 01110001 00101110 0....... ........ */
                /* la.decode:1400 */
                return LISA_VSIGNCOV_B;
            case 0x5:
                /* 01110001 00101110 1....... ........ */
                /* la.decode:1401 */
                return LISA_VSIGNCOV_H;
            case 0x6:
                /* 01110001 00101111 0....... ........ */
                /* la.decode:1402 */
                return LISA_VSIGNCOV_W;
            case 0x7:
                /* 01110001 00101111 1....... ........ */
                /* la.decode:1403 */
                return LISA_VSIGNCOV_D;
            }
            return LISA_INVALID;
        case 0x4c:
            /* 01110001 001100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00110000 0....... ........ */
                /* la.decode:1404 */
                return LISA_VFADD_H;
            case 0x1:
                /* 01110001 00110000 1....... ........ */
                /* la.decode:1405 */
                return LISA_VFADD_S;
            case 0x2:
                /* 01110001 00110001 0....... ........ */
                /* la.decode:1406 */
                return LISA_VFADD_D;
            case 0x3:
                /* 01110001 00110001 1....... ........ */
                /* la.decode:1407 */
                return LISA_VFADD_Q;
            case 0x4:
                /* 01110001 00110010 0....... ........ */
                /* la.decode:1408 */
                return LISA_VFSUB_H;
            case 0x5:
                /* 01110001 00110010 1....... ........ */
                /* la.decode:1409 */
                return LISA_VFSUB_S;
            case 0x6:
                /* 01110001 00110011 0....... ........ */
                /* la.decode:1410 */
                return LISA_VFSUB_D;
            case 0x7:
                /* 01110001 00110011 1....... ........ */
                /* la.decode:1411 */
                return LISA_VFSUB_Q;
            }
            return LISA_INVALID;
        case 0x4d:
            /* 01110001 001101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00110100 0....... ........ */
                /* la.decode:1412 */
                return LISA_VFADDSUB_H;
            case 0x1:
                /* 01110001 00110100 1....... ........ */
                /* la.decode:1413 */
                return LISA_VFADDSUB_S;
            case 0x2:
                /* 01110001 00110101 0....... ........ */
                /* la.decode:1414 */
                return LISA_VFADDSUB_D;
            case 0x3:
                /* 01110001 00110101 1....... ........ */
                /* la.decode:1415 */
                return LISA_VFADDSUB_Q;
            case 0x4:
                /* 01110001 00110110 0....... ........ */
                /* la.decode:1416 */
                return LISA_VFSUBADD_H;
            case 0x5:
                /* 01110001 00110110 1....... ........ */
                /* la.decode:1417 */
                return LISA_VFSUBADD_S;
            case 0x6:
                /* 01110001 00110111 0....... ........ */
                /* la.decode:1418 */
                return LISA_VFSUBADD_D;
            case 0x7:
                /* 01110001 00110111 1....... ........ */
                /* la.decode:1419 */
                return LISA_VFSUBADD_Q;
            }
            return LISA_INVALID;
        case 0x4e:
            /* 01110001 001110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00111000 0....... ........ */
                /* la.decode:1420 */
                return LISA_VFMUL_H;
            case 0x1:
                /* 01110001 00111000 1....... ........ */
                /* la.decode:1421 */
                return LISA_VFMUL_S;
            case 0x2:
                /* 01110001 00111001 0....... ........ */
                /* la.decode:1422 */
                return LISA_VFMUL_D;
            case 0x3:
                /* 01110001 00111001 1....... ........ */
                /* la.decode:1423 */
                return LISA_VFMUL_Q;
            case 0x4:
                /* 01110001 00111010 0....... ........ */
                /* la.decode:1424 */
                return LISA_VFDIV_H;
            case 0x5:
                /* 01110001 00111010 1....... ........ */
                /* la.decode:1425 */
                return LISA_VFDIV_S;
            case 0x6:
                /* 01110001 00111011 0....... ........ */
                /* la.decode:1426 */
                return LISA_VFDIV_D;
            case 0x7:
                /* 01110001 00111011 1....... ........ */
                /* la.decode:1427 */
                return LISA_VFDIV_Q;
            }
            return LISA_INVALID;
        case 0x4f:
            /* 01110001 001111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 00111100 0....... ........ */
                /* la.decode:1428 */
                return LISA_VFMAX_H;
            case 0x1:
                /* 01110001 00111100 1....... ........ */
                /* la.decode:1429 */
                return LISA_VFMAX_S;
            case 0x2:
                /* 01110001 00111101 0....... ........ */
                /* la.decode:1430 */
                return LISA_VFMAX_D;
            case 0x3:
                /* 01110001 00111101 1....... ........ */
                /* la.decode:1431 */
                return LISA_VFMAX_Q;
            case 0x4:
                /* 01110001 00111110 0....... ........ */
                /* la.decode:1432 */
                return LISA_VFMIN_H;
            case 0x5:
                /* 01110001 00111110 1....... ........ */
                /* la.decode:1433 */
                return LISA_VFMIN_S;
            case 0x6:
                /* 01110001 00111111 0....... ........ */
                /* la.decode:1434 */
                return LISA_VFMIN_D;
            case 0x7:
                /* 01110001 00111111 1....... ........ */
                /* la.decode:1435 */
                return LISA_VFMIN_Q;
            }
            return LISA_INVALID;
        case 0x50:
            /* 01110001 010000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01000000 0....... ........ */
                /* la.decode:1436 */
                return LISA_VFMAXA_H;
            case 0x1:
                /* 01110001 01000000 1....... ........ */
                /* la.decode:1437 */
                return LISA_VFMAXA_S;
            case 0x2:
                /* 01110001 01000001 0....... ........ */
                /* la.decode:1438 */
                return LISA_VFMAXA_D;
            case 0x3:
                /* 01110001 01000001 1....... ........ */
                /* la.decode:1439 */
                return LISA_VFMAXA_Q;
            case 0x4:
                /* 01110001 01000010 0....... ........ */
                /* la.decode:1440 */
                return LISA_VFMINA_H;
            case 0x5:
                /* 01110001 01000010 1....... ........ */
                /* la.decode:1441 */
                return LISA_VFMINA_S;
            case 0x6:
                /* 01110001 01000011 0....... ........ */
                /* la.decode:1442 */
                return LISA_VFMINA_D;
            case 0x7:
                /* 01110001 01000011 1....... ........ */
                /* la.decode:1443 */
                return LISA_VFMINA_Q;
            }
            return LISA_INVALID;
        case 0x51:
            /* 01110001 010001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01000100 0....... ........ */
                /* la.decode:1444 */
                return LISA_VFSCALEB_H;
            case 0x1:
                /* 01110001 01000100 1....... ........ */
                /* la.decode:1445 */
                return LISA_VFSCALEB_S;
            case 0x2:
                /* 01110001 01000101 0....... ........ */
                /* la.decode:1446 */
                return LISA_VFSCALEB_D;
            case 0x3:
                /* 01110001 01000101 1....... ........ */
                /* la.decode:1447 */
                return LISA_VFSCALEB_Q;
            case 0x4:
                /* 01110001 01000110 0....... ........ */
                /* la.decode:1448 */
                return LISA_VFCVT_H_S;
            case 0x5:
                /* 01110001 01000110 1....... ........ */
                /* la.decode:1449 */
                return LISA_VFCVT_S_D;
            case 0x6:
                /* 01110001 01000111 0....... ........ */
                /* la.decode:1450 */
                return LISA_VFCVT_D_Q;
            case 0x7:
                /* 01110001 01000111 1....... ........ */
                /* la.decode:1451 */
                return LISA_VFFINT_H_W;
            }
            return LISA_INVALID;
        case 0x52:
            /* 01110001 010010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01001000 0....... ........ */
                /* la.decode:1452 */
                return LISA_VFFINT_S_L;
            case 0x1:
                /* 01110001 01001000 1....... ........ */
                /* la.decode:1453 */
                return LISA_VFFINT_H_WU;
            case 0x2:
                /* 01110001 01001001 0....... ........ */
                /* la.decode:1454 */
                return LISA_VFFINT_S_LU;
            case 0x3:
                /* 01110001 01001001 1....... ........ */
                /* la.decode:1455 */
                return LISA_VFTINT_W_D;
            case 0x4:
                /* 01110001 01001010 0....... ........ */
                /* la.decode:1456 */
                return LISA_VFTINTRM_W_D;
            case 0x5:
                /* 01110001 01001010 1....... ........ */
                /* la.decode:1457 */
                return LISA_VFTINTRP_W_D;
            case 0x6:
                /* 01110001 01001011 0....... ........ */
                /* la.decode:1458 */
                return LISA_VFTINTRZ_W_D;
            case 0x7:
                /* 01110001 01001011 1....... ........ */
                /* la.decode:1459 */
                return LISA_VFTINTRNE_W_D;
            }
            return LISA_INVALID;
        case 0x53:
            /* 01110001 010011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01001100 0....... ........ */
                /* la.decode:1460 */
                return LISA_VFTINT_WU_D;
            case 0x1:
                /* 01110001 01001100 1....... ........ */
                /* la.decode:1461 */
                return LISA_VFTINTRM_WU_D;
            case 0x2:
                /* 01110001 01001101 0....... ........ */
                /* la.decode:1462 */
                return LISA_VFTINTRP_WU_D;
            case 0x3:
                /* 01110001 01001101 1....... ........ */
                /* la.decode:1463 */
                return LISA_VFTINTRZ_WU_D;
            case 0x4:
                /* 01110001 01001110 0....... ........ */
                /* la.decode:1464 */
                return LISA_VFTINTRNE_WU_D;
            case 0x5:
                /* 01110001 01001110 1....... ........ */
                /* la.decode:1465 */
                return LISA_VHADD4_H_BU;
            case 0x6:
                /* 01110001 01001111 0....... ........ */
                /* la.decode:1466 */
                return LISA_VSHUF4_W;
            case 0x7:
                /* 01110001 01001111 1....... ........ */
                /* la.decode:1467 */
                return LISA_VSHUF2_D;
            }
            return LISA_INVALID;
        case 0x54:
            /* 01110001 010100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01010000 0....... ........ */
                /* la.decode:1468 */
                return LISA_AES128_ENC;
            case 0x1:
                /* 01110001 01010000 1....... ........ */
                /* la.decode:1469 */
                return LISA_AES128_DEC;
            case 0x2:
                /* 01110001 01010001 0....... ........ */
                /* la.decode:1470 */
                return LISA_AES192_ENC;
            case 0x3:
                /* 01110001 01010001 1....... ........ */
                /* la.decode:1471 */
                return LISA_AES192_DEC;
            case 0x4:
                /* 01110001 01010010 0....... ........ */
                /* la.decode:1472 */
                return LISA_AES256_ENC;
            case 0x5:
                /* 01110001 01010010 1....... ........ */
                /* la.decode:1473 */
                return LISA_AES256_DEC;
            case 0x6:
                /* 01110001 01010011 0....... ........ */
                /* la.decode:1474 */
                return LISA_AES_KG;
            case 0x7:
                /* 01110001 01010011 1....... ........ */
                /* la.decode:1475 */
                return LISA_AES_FR_ENC;
            }
            return LISA_INVALID;
        case 0x55:
            /* 01110001 010101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01010100 0....... ........ */
                /* la.decode:1476 */
                return LISA_AES_FR_DEC;
            case 0x1:
                /* 01110001 01010100 1....... ........ */
                /* la.decode:1477 */
                return LISA_AES_LR_ENC;
            case 0x2:
                /* 01110001 01010101 0....... ........ */
                /* la.decode:1478 */
                return LISA_AES_LR_DEC;
            case 0x3:
                /* 01110001 01010101 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01010101 100000.. ........ */
                    /* la.decode:1479 */
                    return LISA_AES_MC_ENC;
                }
                return LISA_INVALID;
            case 0x4:
                /* 01110001 01010110 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01010110 000000.. ........ */
                    /* la.decode:1480 */
                    return LISA_AES_MC_DEC;
                }
                return LISA_INVALID;
            case 0x5:
                /* 01110001 01010110 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01010110 100000.. ........ */
                    /* la.decode:1481 */
                    return LISA_AES_SB_ENC;
                }
                return LISA_INVALID;
            case 0x6:
                /* 01110001 01010111 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01010111 000000.. ........ */
                    /* la.decode:1482 */
                    return LISA_AES_SB_DEC;
                }
                return LISA_INVALID;
            case 0x7:
                /* 01110001 01010111 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01010111 100000.. ........ */
                    /* la.decode:1483 */
                    return LISA_AES_SR_ENC;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0x56:
            /* 01110001 010110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01011000 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110001 01011000 000000.. ........ */
                    /* la.decode:1484 */
                    return LISA_AES_SR_DEC;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110001 01011000 1....... ........ */
                /* la.decode:1485 */
                return LISA_MD5_MS;
            case 0x2:
                /* 01110001 01011001 0....... ........ */
                /* la.decode:1486 */
                return LISA_MD5_4R;
            case 0x3:
                /* 01110001 01011001 1....... ........ */
                /* la.decode:1487 */
                return LISA_SHA1_MS_1;
            case 0x4:
                /* 01110001 01011010 0....... ........ */
                /* la.decode:1488 */
                return LISA_SHA1_MS_2;
            case 0x5:
                /* 01110001 01011010 1....... ........ */
                /* la.decode:1489 */
                return LISA_SHA1_HASH_4R;
            case 0x6:
                /* 01110001 01011011 0....... ........ */
                /* la.decode:1490 */
                return LISA_SHA256_MS_1;
            case 0x7:
                /* 01110001 01011011 1....... ........ */
                /* la.decode:1491 */
                return LISA_SHA256_MS_2;
            }
            return LISA_INVALID;
        case 0x57:
            /* 01110001 010111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01011100 0....... ........ */
                /* la.decode:1492 */
                return LISA_SHA256_HASH_2R;
            case 0x1:
                /* 01110001 01011100 1....... ........ */
                /* la.decode:1493 */
                return LISA_SHA512_MS_1;
            case 0x2:
                /* 01110001 01011101 0....... ........ */
                /* la.decode:1494 */
                return LISA_SHA512_MS_2;
            case 0x3:
                /* 01110001 01011101 1....... ........ */
                /* la.decode:1495 */
                return LISA_SHA512_HASH_R_1;
            case 0x4:
                /* 01110001 01011110 0....... ........ */
                /* la.decode:1496 */
                return LISA_SHA512_HASH_R_2;
            }
            return LISA_INVALID;
        case 0x58:
            /* 01110001 011000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01100000 0....... ........ */
                /* la.decode:1497 */
                return LISA_VPMUL_W;
            case 0x1:
                /* 01110001 01100000 1....... ........ */
                /* la.decode:1498 */
                return LISA_VPMUL_D;
            case 0x2:
                /* 01110001 01100001 0....... ........ */
                /* la.decode:1499 */
                return LISA_VPMUH_W;
            case 0x3:
                /* 01110001 01100001 1....... ........ */
                /* la.decode:1500 */
                return LISA_VPMUH_D;
            case 0x4:
                /* 01110001 01100010 0....... ........ */
                /* la.decode:1501 */
                return LISA_VPMULACC_W;
            case 0x5:
                /* 01110001 01100010 1....... ........ */
                /* la.decode:1502 */
                return LISA_VPMULACC_D;
            case 0x6:
                /* 01110001 01100011 0....... ........ */
                /* la.decode:1503 */
                return LISA_VPMUHACC_W;
            case 0x7:
                /* 01110001 01100011 1....... ........ */
                /* la.decode:1504 */
                return LISA_VPMUHACC_D;
            }
            return LISA_INVALID;
        case 0x59:
            /* 01110001 011001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01100100 0....... ........ */
                /* la.decode:1505 */
                return LISA_VPMULWL_H_B;
            case 0x1:
                /* 01110001 01100100 1....... ........ */
                /* la.decode:1506 */
                return LISA_VPMULWL_W_H;
            case 0x2:
                /* 01110001 01100101 0....... ........ */
                /* la.decode:1507 */
                return LISA_VPMULWL_D_W;
            case 0x3:
                /* 01110001 01100101 1....... ........ */
                /* la.decode:1508 */
                return LISA_VPMULWL_Q_D;
            case 0x4:
                /* 01110001 01100110 0....... ........ */
                /* la.decode:1509 */
                return LISA_VPMULWH_H_B;
            case 0x5:
                /* 01110001 01100110 1....... ........ */
                /* la.decode:1510 */
                return LISA_VPMULWH_W_H;
            case 0x6:
                /* 01110001 01100111 0....... ........ */
                /* la.decode:1511 */
                return LISA_VPMULWH_D_W;
            case 0x7:
                /* 01110001 01100111 1....... ........ */
                /* la.decode:1512 */
                return LISA_VPMULWH_Q_D;
            }
            return LISA_INVALID;
        case 0x5a:
            /* 01110001 011010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01101000 0....... ........ */
                /* la.decode:1513 */
                return LISA_VPMADDWL_H_B;
            case 0x1:
                /* 01110001 01101000 1....... ........ */
                /* la.decode:1514 */
                return LISA_VPMADDWL_W_H;
            case 0x2:
                /* 01110001 01101001 0....... ........ */
                /* la.decode:1515 */
                return LISA_VPMADDWL_D_W;
            case 0x3:
                /* 01110001 01101001 1....... ........ */
                /* la.decode:1516 */
                return LISA_VPMADDWL_Q_D;
            case 0x4:
                /* 01110001 01101010 0....... ........ */
                /* la.decode:1517 */
                return LISA_VPMADDWH_H_B;
            case 0x5:
                /* 01110001 01101010 1....... ........ */
                /* la.decode:1518 */
                return LISA_VPMADDWH_W_H;
            case 0x6:
                /* 01110001 01101011 0....... ........ */
                /* la.decode:1519 */
                return LISA_VPMADDWH_D_W;
            case 0x7:
                /* 01110001 01101011 1....... ........ */
                /* la.decode:1520 */
                return LISA_VPMADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0x5b:
            /* 01110001 011011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01101100 0....... ........ */
                /* la.decode:1521 */
                return LISA_VPDP2_Q_D;
            case 0x1:
                /* 01110001 01101100 1....... ........ */
                /* la.decode:1522 */
                return LISA_VPDP2ADD_Q_D;
            }
            return LISA_INVALID;
        case 0x5c:
            /* 01110001 011100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110001 01110000 0....... ........ */
                /* la.decode:1523 */
                return LISA_VCDP4_RE_D_H;
            case 0x1:
                /* 01110001 01110000 1....... ........ */
                /* la.decode:1524 */
                return LISA_VCDP4_IM_D_H;
            case 0x2:
                /* 01110001 01110001 0....... ........ */
                /* la.decode:1525 */
                return LISA_VCDP4ADD_RE_D_H;
            case 0x3:
                /* 01110001 01110001 1....... ........ */
                /* la.decode:1526 */
                return LISA_VCDP4ADD_IM_D_H;
            case 0x4:
                /* 01110001 01110010 0....... ........ */
                /* la.decode:1527 */
                return LISA_VCDP2_RE_Q_W;
            case 0x5:
                /* 01110001 01110010 1....... ........ */
                /* la.decode:1528 */
                return LISA_VCDP2_IM_Q_W;
            case 0x6:
                /* 01110001 01110011 0....... ........ */
                /* la.decode:1529 */
                return LISA_VCDP2ADD_RE_Q_W;
            case 0x7:
                /* 01110001 01110011 1....... ........ */
                /* la.decode:1530 */
                return LISA_VCDP2ADD_IM_Q_W;
            }
            return LISA_INVALID;
        case 0x5e:
            /* 01110001 011110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x2:
                /* 01110001 01111001 0....... ........ */
                /* la.decode:1531 */
                return LISA_VSIGNSEL_W;
            case 0x3:
                /* 01110001 01111001 1....... ........ */
                /* la.decode:1532 */
                return LISA_VSIGNSEL_D;
            case 0x5:
                /* 01110001 01111010 1....... ........ */
                /* la.decode:1533 */
                return LISA_VSHUF_H;
            case 0x6:
                /* 01110001 01111011 0....... ........ */
                /* la.decode:1534 */
                return LISA_VSHUF_W;
            case 0x7:
                /* 01110001 01111011 1....... ........ */
                /* la.decode:1535 */
                return LISA_VSHUF_D;
            }
            return LISA_INVALID;
        case 0xa0:
            /* 01110010 100000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10000000 0....... ........ */
                /* la.decode:1536 */
                return LISA_VSEQI_B;
            case 0x1:
                /* 01110010 10000000 1....... ........ */
                /* la.decode:1537 */
                return LISA_VSEQI_H;
            case 0x2:
                /* 01110010 10000001 0....... ........ */
                /* la.decode:1538 */
                return LISA_VSEQI_W;
            case 0x3:
                /* 01110010 10000001 1....... ........ */
                /* la.decode:1539 */
                return LISA_VSEQI_D;
            case 0x4:
                /* 01110010 10000010 0....... ........ */
                /* la.decode:1540 */
                return LISA_VSLEI_B;
            case 0x5:
                /* 01110010 10000010 1....... ........ */
                /* la.decode:1541 */
                return LISA_VSLEI_H;
            case 0x6:
                /* 01110010 10000011 0....... ........ */
                /* la.decode:1542 */
                return LISA_VSLEI_W;
            case 0x7:
                /* 01110010 10000011 1....... ........ */
                /* la.decode:1543 */
                return LISA_VSLEI_D;
            }
            return LISA_INVALID;
        case 0xa1:
            /* 01110010 100001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10000100 0....... ........ */
                /* la.decode:1544 */
                return LISA_VSLEI_BU;
            case 0x1:
                /* 01110010 10000100 1....... ........ */
                /* la.decode:1545 */
                return LISA_VSLEI_HU;
            case 0x2:
                /* 01110010 10000101 0....... ........ */
                /* la.decode:1546 */
                return LISA_VSLEI_WU;
            case 0x3:
                /* 01110010 10000101 1....... ........ */
                /* la.decode:1547 */
                return LISA_VSLEI_DU;
            case 0x4:
                /* 01110010 10000110 0....... ........ */
                /* la.decode:1548 */
                return LISA_VSLTI_B;
            case 0x5:
                /* 01110010 10000110 1....... ........ */
                /* la.decode:1549 */
                return LISA_VSLTI_H;
            case 0x6:
                /* 01110010 10000111 0....... ........ */
                /* la.decode:1550 */
                return LISA_VSLTI_W;
            case 0x7:
                /* 01110010 10000111 1....... ........ */
                /* la.decode:1551 */
                return LISA_VSLTI_D;
            }
            return LISA_INVALID;
        case 0xa2:
            /* 01110010 100010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10001000 0....... ........ */
                /* la.decode:1552 */
                return LISA_VSLTI_BU;
            case 0x1:
                /* 01110010 10001000 1....... ........ */
                /* la.decode:1553 */
                return LISA_VSLTI_HU;
            case 0x2:
                /* 01110010 10001001 0....... ........ */
                /* la.decode:1554 */
                return LISA_VSLTI_WU;
            case 0x3:
                /* 01110010 10001001 1....... ........ */
                /* la.decode:1555 */
                return LISA_VSLTI_DU;
            case 0x4:
                /* 01110010 10001010 0....... ........ */
                /* la.decode:1556 */
                return LISA_VADDI_BU;
            case 0x5:
                /* 01110010 10001010 1....... ........ */
                /* la.decode:1557 */
                return LISA_VADDI_HU;
            case 0x6:
                /* 01110010 10001011 0....... ........ */
                /* la.decode:1558 */
                return LISA_VADDI_WU;
            case 0x7:
                /* 01110010 10001011 1....... ........ */
                /* la.decode:1559 */
                return LISA_VADDI_DU;
            }
            return LISA_INVALID;
        case 0xa3:
            /* 01110010 100011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10001100 0....... ........ */
                /* la.decode:1560 */
                return LISA_VSUBI_BU;
            case 0x1:
                /* 01110010 10001100 1....... ........ */
                /* la.decode:1561 */
                return LISA_VSUBI_HU;
            case 0x2:
                /* 01110010 10001101 0....... ........ */
                /* la.decode:1562 */
                return LISA_VSUBI_WU;
            case 0x3:
                /* 01110010 10001101 1....... ........ */
                /* la.decode:1563 */
                return LISA_VSUBI_DU;
            case 0x4:
                /* 01110010 10001110 0....... ........ */
                /* la.decode:1564 */
                return LISA_VBSLL_V;
            case 0x5:
                /* 01110010 10001110 1....... ........ */
                /* la.decode:1565 */
                return LISA_VBSRL_V;
            }
            return LISA_INVALID;
        case 0xa4:
            /* 01110010 100100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10010000 0....... ........ */
                /* la.decode:1566 */
                return LISA_VMAXI_B;
            case 0x1:
                /* 01110010 10010000 1....... ........ */
                /* la.decode:1567 */
                return LISA_VMAXI_H;
            case 0x2:
                /* 01110010 10010001 0....... ........ */
                /* la.decode:1568 */
                return LISA_VMAXI_W;
            case 0x3:
                /* 01110010 10010001 1....... ........ */
                /* la.decode:1569 */
                return LISA_VMAXI_D;
            case 0x4:
                /* 01110010 10010010 0....... ........ */
                /* la.decode:1570 */
                return LISA_VMINI_B;
            case 0x5:
                /* 01110010 10010010 1....... ........ */
                /* la.decode:1571 */
                return LISA_VMINI_H;
            case 0x6:
                /* 01110010 10010011 0....... ........ */
                /* la.decode:1572 */
                return LISA_VMINI_W;
            case 0x7:
                /* 01110010 10010011 1....... ........ */
                /* la.decode:1573 */
                return LISA_VMINI_D;
            }
            return LISA_INVALID;
        case 0xa5:
            /* 01110010 100101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10010100 0....... ........ */
                /* la.decode:1574 */
                return LISA_VMAXI_BU;
            case 0x1:
                /* 01110010 10010100 1....... ........ */
                /* la.decode:1575 */
                return LISA_VMAXI_HU;
            case 0x2:
                /* 01110010 10010101 0....... ........ */
                /* la.decode:1576 */
                return LISA_VMAXI_WU;
            case 0x3:
                /* 01110010 10010101 1....... ........ */
                /* la.decode:1577 */
                return LISA_VMAXI_DU;
            case 0x4:
                /* 01110010 10010110 0....... ........ */
                /* la.decode:1578 */
                return LISA_VMINI_BU;
            case 0x5:
                /* 01110010 10010110 1....... ........ */
                /* la.decode:1579 */
                return LISA_VMINI_HU;
            case 0x6:
                /* 01110010 10010111 0....... ........ */
                /* la.decode:1580 */
                return LISA_VMINI_WU;
            case 0x7:
                /* 01110010 10010111 1....... ........ */
                /* la.decode:1581 */
                return LISA_VMINI_DU;
            }
            return LISA_INVALID;
        case 0xa6:
            /* 01110010 100110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110010 10011000 0....... ........ */
                /* la.decode:1582 */
                return LISA_VRANDSIGNI_B;
            case 0x1:
                /* 01110010 10011000 1....... ........ */
                /* la.decode:1583 */
                return LISA_VRANDSIGNI_H;
            case 0x2:
                /* 01110010 10011001 0....... ........ */
                /* la.decode:1584 */
                return LISA_VRORSIGNI_B;
            case 0x3:
                /* 01110010 10011001 1....... ........ */
                /* la.decode:1585 */
                return LISA_VRORSIGNI_H;
            case 0x4:
                /* 01110010 10011010 0....... ........ */
                /* la.decode:1586 */
                return LISA_VFRSTPI_B;
            case 0x5:
                /* 01110010 10011010 1....... ........ */
                /* la.decode:1587 */
                return LISA_VFRSTPI_H;
            case 0x6:
                /* 01110010 10011011 0....... ........ */
                /* la.decode:1588 */
                return LISA_VCLRSTRI_V;
            case 0x7:
                /* 01110010 10011011 1....... ........ */
                /* la.decode:1589 */
                return LISA_VMEPATMSK_V;
            }
            return LISA_INVALID;
        case 0xa7:
            /* 01110010 100111.. ........ ........ */
            switch ((insn >> 10) & 0xff) {
            case 0x0:
                /* 01110010 10011100 000000.. ........ */
                /* la.decode:1590 */
                return LISA_VCLO_B;
            case 0x1:
                /* 01110010 10011100 000001.. ........ */
                /* la.decode:1591 */
                return LISA_VCLO_H;
            case 0x2:
                /* 01110010 10011100 000010.. ........ */
                /* la.decode:1592 */
                return LISA_VCLO_W;
            case 0x3:
                /* 01110010 10011100 000011.. ........ */
                /* la.decode:1593 */
                return LISA_VCLO_D;
            case 0x4:
                /* 01110010 10011100 000100.. ........ */
                /* la.decode:1594 */
                return LISA_VCLZ_B;
            case 0x5:
                /* 01110010 10011100 000101.. ........ */
                /* la.decode:1595 */
                return LISA_VCLZ_H;
            case 0x6:
                /* 01110010 10011100 000110.. ........ */
                /* la.decode:1596 */
                return LISA_VCLZ_W;
            case 0x7:
                /* 01110010 10011100 000111.. ........ */
                /* la.decode:1597 */
                return LISA_VCLZ_D;
            case 0x8:
                /* 01110010 10011100 001000.. ........ */
                /* la.decode:1598 */
                return LISA_VPCNT_B;
            case 0x9:
                /* 01110010 10011100 001001.. ........ */
                /* la.decode:1599 */
                return LISA_VPCNT_H;
            case 0xa:
                /* 01110010 10011100 001010.. ........ */
                /* la.decode:1600 */
                return LISA_VPCNT_W;
            case 0xb:
                /* 01110010 10011100 001011.. ........ */
                /* la.decode:1601 */
                return LISA_VPCNT_D;
            case 0xc:
                /* 01110010 10011100 001100.. ........ */
                /* la.decode:1602 */
                return LISA_VNEG_B;
            case 0xd:
                /* 01110010 10011100 001101.. ........ */
                /* la.decode:1603 */
                return LISA_VNEG_H;
            case 0xe:
                /* 01110010 10011100 001110.. ........ */
                /* la.decode:1604 */
                return LISA_VNEG_W;
            case 0xf:
                /* 01110010 10011100 001111.. ........ */
                /* la.decode:1605 */
                return LISA_VNEG_D;
            case 0x10:
                /* 01110010 10011100 010000.. ........ */
                /* la.decode:1606 */
                return LISA_VMSKLTZ_B;
            case 0x11:
                /* 01110010 10011100 010001.. ........ */
                /* la.decode:1607 */
                return LISA_VMSKLTZ_H;
            case 0x12:
                /* 01110010 10011100 010010.. ........ */
                /* la.decode:1608 */
                return LISA_VMSKLTZ_W;
            case 0x13:
                /* 01110010 10011100 010011.. ........ */
                /* la.decode:1609 */
                return LISA_VMSKLTZ_D;
            case 0x14:
                /* 01110010 10011100 010100.. ........ */
                /* la.decode:1610 */
                return LISA_VMSKGEZ_B;
            case 0x15:
                /* 01110010 10011100 010101.. ........ */
                /* la.decode:1611 */
                return LISA_VMSKGEZ_H;
            case 0x16:
                /* 01110010 10011100 010110.. ........ */
                /* la.decode:1612 */
                return LISA_VMSKGEZ_W;
            case 0x17:
                /* 01110010 10011100 010111.. ........ */
                /* la.decode:1613 */
                return LISA_VMSKGEZ_D;
            case 0x18:
                /* 01110010 10011100 011000.. ........ */
                /* la.decode:1614 */
                return LISA_VMSKNZ_B;
            case 0x19:
                /* 01110010 10011100 011001.. ........ */
                /* la.decode:1615 */
                return LISA_VMSKNZ_H;
            case 0x1a:
                /* 01110010 10011100 011010.. ........ */
                /* la.decode:1616 */
                return LISA_VMSKNZ_W;
            case 0x1b:
                /* 01110010 10011100 011011.. ........ */
                /* la.decode:1617 */
                return LISA_VMSKNZ_D;
            case 0x1c:
                /* 01110010 10011100 011100.. ........ */
                /* la.decode:1618 */
                return LISA_VMSKCOPY_B;
            case 0x1d:
                /* 01110010 10011100 011101.. ........ */
                /* la.decode:1619 */
                return LISA_VMSKCOPY_H;
            case 0x1e:
                /* 01110010 10011100 011110.. ........ */
                /* la.decode:1620 */
                return LISA_VMSKCOPY_W;
            case 0x1f:
                /* 01110010 10011100 011111.. ........ */
                /* la.decode:1621 */
                return LISA_VMSKCOPY_D;
            case 0x20:
                /* 01110010 10011100 100000.. ........ */
                /* la.decode:1622 */
                return LISA_VMSKFILL_B;
            case 0x21:
                /* 01110010 10011100 100001.. ........ */
                /* la.decode:1623 */
                return LISA_VMSKFILL_H;
            case 0x22:
                /* 01110010 10011100 100010.. ........ */
                /* la.decode:1624 */
                return LISA_VMSKFILL_W;
            case 0x23:
                /* 01110010 10011100 100011.. ........ */
                /* la.decode:1625 */
                return LISA_VMSKFILL_D;
            case 0x24:
                /* 01110010 10011100 100100.. ........ */
                /* la.decode:1626 */
                return LISA_VFRSTM_B;
            case 0x25:
                /* 01110010 10011100 100101.. ........ */
                /* la.decode:1627 */
                return LISA_VFRSTM_H;
            case 0x26:
                /* 01110010 10011100 100110.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 100110.. ...00... */
                    /* la.decode:1628 */
                    return LISA_VSETEQZ_V;
                }
                return LISA_INVALID;
            case 0x27:
                /* 01110010 10011100 100111.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 100111.. ...00... */
                    /* la.decode:1629 */
                    return LISA_VSETNEZ_V;
                }
                return LISA_INVALID;
            case 0x28:
                /* 01110010 10011100 101000.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101000.. ...00... */
                    /* la.decode:1630 */
                    return LISA_VSETANYEQZ_B;
                }
                return LISA_INVALID;
            case 0x29:
                /* 01110010 10011100 101001.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101001.. ...00... */
                    /* la.decode:1631 */
                    return LISA_VSETANYEQZ_H;
                }
                return LISA_INVALID;
            case 0x2a:
                /* 01110010 10011100 101010.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101010.. ...00... */
                    /* la.decode:1632 */
                    return LISA_VSETANYEQZ_W;
                }
                return LISA_INVALID;
            case 0x2b:
                /* 01110010 10011100 101011.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101011.. ...00... */
                    /* la.decode:1633 */
                    return LISA_VSETANYEQZ_D;
                }
                return LISA_INVALID;
            case 0x2c:
                /* 01110010 10011100 101100.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101100.. ...00... */
                    /* la.decode:1634 */
                    return LISA_VSETALLNEZ_B;
                }
                return LISA_INVALID;
            case 0x2d:
                /* 01110010 10011100 101101.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101101.. ...00... */
                    /* la.decode:1635 */
                    return LISA_VSETALLNEZ_H;
                }
                return LISA_INVALID;
            case 0x2e:
                /* 01110010 10011100 101110.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101110.. ...00... */
                    /* la.decode:1636 */
                    return LISA_VSETALLNEZ_W;
                }
                return LISA_INVALID;
            case 0x2f:
                /* 01110010 10011100 101111.. ........ */
                switch ((insn >> 3) & 0x3) {
                case 0x0:
                    /* 01110010 10011100 101111.. ...00... */
                    /* la.decode:1637 */
                    return LISA_VSETALLNEZ_D;
                }
                return LISA_INVALID;
            case 0x30:
                /* 01110010 10011100 110000.. ........ */
                /* la.decode:1638 */
                return LISA_VFLOGB_H;
            case 0x31:
                /* 01110010 10011100 110001.. ........ */
                /* la.decode:1639 */
                return LISA_VFLOGB_S;
            case 0x32:
                /* 01110010 10011100 110010.. ........ */
                /* la.decode:1640 */
                return LISA_VFLOGB_D;
            case 0x33:
                /* 01110010 10011100 110011.. ........ */
                /* la.decode:1641 */
                return LISA_VFLOGB_Q;
            case 0x34:
                /* 01110010 10011100 110100.. ........ */
                /* la.decode:1642 */
                return LISA_VFCLASS_H;
            case 0x35:
                /* 01110010 10011100 110101.. ........ */
                /* la.decode:1643 */
                return LISA_VFCLASS_S;
            case 0x36:
                /* 01110010 10011100 110110.. ........ */
                /* la.decode:1644 */
                return LISA_VFCLASS_D;
            case 0x37:
                /* 01110010 10011100 110111.. ........ */
                /* la.decode:1645 */
                return LISA_VFCLASS_Q;
            case 0x38:
                /* 01110010 10011100 111000.. ........ */
                /* la.decode:1646 */
                return LISA_VFSQRT_H;
            case 0x39:
                /* 01110010 10011100 111001.. ........ */
                /* la.decode:1647 */
                return LISA_VFSQRT_S;
            case 0x3a:
                /* 01110010 10011100 111010.. ........ */
                /* la.decode:1648 */
                return LISA_VFSQRT_D;
            case 0x3b:
                /* 01110010 10011100 111011.. ........ */
                /* la.decode:1649 */
                return LISA_VFSQRT_Q;
            case 0x3c:
                /* 01110010 10011100 111100.. ........ */
                /* la.decode:1650 */
                return LISA_VFRECIP_H;
            case 0x3d:
                /* 01110010 10011100 111101.. ........ */
                /* la.decode:1651 */
                return LISA_VFRECIP_S;
            case 0x3e:
                /* 01110010 10011100 111110.. ........ */
                /* la.decode:1652 */
                return LISA_VFRECIP_D;
            case 0x3f:
                /* 01110010 10011100 111111.. ........ */
                /* la.decode:1653 */
                return LISA_VFRECIP_Q;
            case 0x40:
                /* 01110010 10011101 000000.. ........ */
                /* la.decode:1654 */
                return LISA_VFRSQRT_H;
            case 0x41:
                /* 01110010 10011101 000001.. ........ */
                /* la.decode:1655 */
                return LISA_VFRSQRT_S;
            case 0x42:
                /* 01110010 10011101 000010.. ........ */
                /* la.decode:1656 */
                return LISA_VFRSQRT_D;
            case 0x43:
                /* 01110010 10011101 000011.. ........ */
                /* la.decode:1657 */
                return LISA_VFRSQRT_Q;
            case 0x44:
                /* 01110010 10011101 000100.. ........ */
                /* la.decode:1658 */
                return LISA_VFRECIPE_H;
            case 0x45:
                /* 01110010 10011101 000101.. ........ */
                /* la.decode:1659 */
                return LISA_VFRECIPE_S;
            case 0x46:
                /* 01110010 10011101 000110.. ........ */
                /* la.decode:1660 */
                return LISA_VFRECIPE_D;
            case 0x47:
                /* 01110010 10011101 000111.. ........ */
                /* la.decode:1661 */
                return LISA_VFRECIPE_Q;
            case 0x48:
                /* 01110010 10011101 001000.. ........ */
                /* la.decode:1662 */
                return LISA_VFRSQRTE_H;
            case 0x49:
                /* 01110010 10011101 001001.. ........ */
                /* la.decode:1663 */
                return LISA_VFRSQRTE_S;
            case 0x4a:
                /* 01110010 10011101 001010.. ........ */
                /* la.decode:1664 */
                return LISA_VFRSQRTE_D;
            case 0x4b:
                /* 01110010 10011101 001011.. ........ */
                /* la.decode:1665 */
                return LISA_VFRSQRTE_Q;
            case 0x4c:
                /* 01110010 10011101 001100.. ........ */
                /* la.decode:1666 */
                return LISA_VFRINT_H;
            case 0x4d:
                /* 01110010 10011101 001101.. ........ */
                /* la.decode:1667 */
                return LISA_VFRINT_S;
            case 0x4e:
                /* 01110010 10011101 001110.. ........ */
                /* la.decode:1668 */
                return LISA_VFRINT_D;
            case 0x4f:
                /* 01110010 10011101 001111.. ........ */
                /* la.decode:1669 */
                return LISA_VFRINT_Q;
            case 0x50:
                /* 01110010 10011101 010000.. ........ */
                /* la.decode:1670 */
                return LISA_VFRINTRM_H;
            case 0x51:
                /* 01110010 10011101 010001.. ........ */
                /* la.decode:1671 */
                return LISA_VFRINTRM_S;
            case 0x52:
                /* 01110010 10011101 010010.. ........ */
                /* la.decode:1672 */
                return LISA_VFRINTRM_D;
            case 0x53:
                /* 01110010 10011101 010011.. ........ */
                /* la.decode:1673 */
                return LISA_VFRINTRM_Q;
            case 0x54:
                /* 01110010 10011101 010100.. ........ */
                /* la.decode:1674 */
                return LISA_VFRINTRP_H;
            case 0x55:
                /* 01110010 10011101 010101.. ........ */
                /* la.decode:1675 */
                return LISA_VFRINTRP_S;
            case 0x56:
                /* 01110010 10011101 010110.. ........ */
                /* la.decode:1676 */
                return LISA_VFRINTRP_D;
            case 0x57:
                /* 01110010 10011101 010111.. ........ */
                /* la.decode:1677 */
                return LISA_VFRINTRP_Q;
            case 0x58:
                /* 01110010 10011101 011000.. ........ */
                /* la.decode:1678 */
                return LISA_VFRINTRZ_H;
            case 0x59:
                /* 01110010 10011101 011001.. ........ */
                /* la.decode:1679 */
                return LISA_VFRINTRZ_S;
            case 0x5a:
                /* 01110010 10011101 011010.. ........ */
                /* la.decode:1680 */
                return LISA_VFRINTRZ_D;
            case 0x5b:
                /* 01110010 10011101 011011.. ........ */
                /* la.decode:1681 */
                return LISA_VFRINTRZ_Q;
            case 0x5c:
                /* 01110010 10011101 011100.. ........ */
                /* la.decode:1682 */
                return LISA_VFRINTRNE_H;
            case 0x5d:
                /* 01110010 10011101 011101.. ........ */
                /* la.decode:1683 */
                return LISA_VFRINTRNE_S;
            case 0x5e:
                /* 01110010 10011101 011110.. ........ */
                /* la.decode:1684 */
                return LISA_VFRINTRNE_D;
            case 0x5f:
                /* 01110010 10011101 011111.. ........ */
                /* la.decode:1685 */
                return LISA_VFRINTRNE_Q;
            case 0x61:
                /* 01110010 10011101 100001.. ........ */
                /* la.decode:1686 */
                return LISA_VEXTL_W_B;
            case 0x62:
                /* 01110010 10011101 100010.. ........ */
                /* la.decode:1687 */
                return LISA_VEXTL_D_B;
            case 0x63:
                /* 01110010 10011101 100011.. ........ */
                /* la.decode:1688 */
                return LISA_VEXTL_Q_B;
            case 0x65:
                /* 01110010 10011101 100101.. ........ */
                /* la.decode:1689 */
                return LISA_VEXTL_D_H;
            case 0x66:
                /* 01110010 10011101 100110.. ........ */
                /* la.decode:1690 */
                return LISA_VEXTL_Q_H;
            case 0x68:
                /* 01110010 10011101 101000.. ........ */
                /* la.decode:1691 */
                return LISA_VEXTL_Q_W;
            case 0x6b:
                /* 01110010 10011101 101011.. ........ */
                /* la.decode:1692 */
                return LISA_VEXTL_W_BU;
            case 0x6c:
                /* 01110010 10011101 101100.. ........ */
                /* la.decode:1693 */
                return LISA_VEXTL_D_BU;
            case 0x6d:
                /* 01110010 10011101 101101.. ........ */
                /* la.decode:1694 */
                return LISA_VEXTL_Q_BU;
            case 0x6f:
                /* 01110010 10011101 101111.. ........ */
                /* la.decode:1695 */
                return LISA_VEXTL_D_HU;
            case 0x70:
                /* 01110010 10011101 110000.. ........ */
                /* la.decode:1696 */
                return LISA_VEXTL_Q_HU;
            case 0x72:
                /* 01110010 10011101 110010.. ........ */
                /* la.decode:1697 */
                return LISA_VEXTL_Q_WU;
            case 0x74:
                /* 01110010 10011101 110100.. ........ */
                /* la.decode:1698 */
                return LISA_VHADD8_D_BU;
            case 0x75:
                /* 01110010 10011101 110101.. ........ */
                /* la.decode:1699 */
                return LISA_VHMINPOS_W_HU;
            case 0x76:
                /* 01110010 10011101 110110.. ........ */
                /* la.decode:1700 */
                return LISA_VHMINPOS_D_HU;
            case 0x77:
                /* 01110010 10011101 110111.. ........ */
                /* la.decode:1701 */
                return LISA_VHMINPOS_Q_HU;
            case 0x78:
                /* 01110010 10011101 111000.. ........ */
                /* la.decode:1702 */
                return LISA_VCLRTAIL_B;
            case 0x79:
                /* 01110010 10011101 111001.. ........ */
                /* la.decode:1703 */
                return LISA_VCLRTAIL_H;
            case 0x7a:
                /* 01110010 10011101 111010.. ........ */
                /* la.decode:1704 */
                return LISA_VFCVTL_S_H;
            case 0x7b:
                /* 01110010 10011101 111011.. ........ */
                /* la.decode:1705 */
                return LISA_VFCVTH_S_H;
            case 0x7c:
                /* 01110010 10011101 111100.. ........ */
                /* la.decode:1706 */
                return LISA_VFCVTL_D_S;
            case 0x7d:
                /* 01110010 10011101 111101.. ........ */
                /* la.decode:1707 */
                return LISA_VFCVTH_D_S;
            case 0x7e:
                /* 01110010 10011101 111110.. ........ */
                /* la.decode:1708 */
                return LISA_VFCVTL_Q_D;
            case 0x7f:
                /* 01110010 10011101 111111.. ........ */
                /* la.decode:1709 */
                return LISA_VFCVTH_Q_D;
            case 0x80:
                /* 01110010 10011110 000000.. ........ */
                /* la.decode:1710 */
                return LISA_VFFINT_S_W;
            case 0x81:
                /* 01110010 10011110 000001.. ........ */
                /* la.decode:1711 */
                return LISA_VFFINT_S_WU;
            case 0x82:
                /* 01110010 10011110 000010.. ........ */
                /* la.decode:1712 */
                return LISA_VFFINT_D_L;
            case 0x83:
                /* 01110010 10011110 000011.. ........ */
                /* la.decode:1713 */
                return LISA_VFFINT_D_LU;
            case 0x84:
                /* 01110010 10011110 000100.. ........ */
                /* la.decode:1714 */
                return LISA_VFFINTL_D_W;
            case 0x85:
                /* 01110010 10011110 000101.. ........ */
                /* la.decode:1715 */
                return LISA_VFFINTH_D_W;
            case 0x86:
                /* 01110010 10011110 000110.. ........ */
                /* la.decode:1716 */
                return LISA_VFFINTL_Q_L;
            case 0x87:
                /* 01110010 10011110 000111.. ........ */
                /* la.decode:1717 */
                return LISA_VFFINTH_Q_L;
            case 0x88:
                /* 01110010 10011110 001000.. ........ */
                /* la.decode:1718 */
                return LISA_VFFINTL_D_WU;
            case 0x89:
                /* 01110010 10011110 001001.. ........ */
                /* la.decode:1719 */
                return LISA_VFFINTH_D_WU;
            case 0x8a:
                /* 01110010 10011110 001010.. ........ */
                /* la.decode:1720 */
                return LISA_VFFINTL_Q_LU;
            case 0x8b:
                /* 01110010 10011110 001011.. ........ */
                /* la.decode:1721 */
                return LISA_VFFINTH_Q_LU;
            case 0x8c:
                /* 01110010 10011110 001100.. ........ */
                /* la.decode:1722 */
                return LISA_VFTINT_W_S;
            case 0x8d:
                /* 01110010 10011110 001101.. ........ */
                /* la.decode:1723 */
                return LISA_VFTINT_L_D;
            case 0x8e:
                /* 01110010 10011110 001110.. ........ */
                /* la.decode:1724 */
                return LISA_VFTINTRM_W_S;
            case 0x8f:
                /* 01110010 10011110 001111.. ........ */
                /* la.decode:1725 */
                return LISA_VFTINTRM_L_D;
            case 0x90:
                /* 01110010 10011110 010000.. ........ */
                /* la.decode:1726 */
                return LISA_VFTINTRP_W_S;
            case 0x91:
                /* 01110010 10011110 010001.. ........ */
                /* la.decode:1727 */
                return LISA_VFTINTRP_L_D;
            case 0x92:
                /* 01110010 10011110 010010.. ........ */
                /* la.decode:1728 */
                return LISA_VFTINTRZ_W_S;
            case 0x93:
                /* 01110010 10011110 010011.. ........ */
                /* la.decode:1729 */
                return LISA_VFTINTRZ_L_D;
            case 0x94:
                /* 01110010 10011110 010100.. ........ */
                /* la.decode:1730 */
                return LISA_VFTINTRNE_W_S;
            case 0x95:
                /* 01110010 10011110 010101.. ........ */
                /* la.decode:1731 */
                return LISA_VFTINTRNE_L_D;
            case 0x96:
                /* 01110010 10011110 010110.. ........ */
                /* la.decode:1732 */
                return LISA_VFTINT_WU_S;
            case 0x97:
                /* 01110010 10011110 010111.. ........ */
                /* la.decode:1733 */
                return LISA_VFTINT_LU_D;
            case 0x98:
                /* 01110010 10011110 011000.. ........ */
                /* la.decode:1734 */
                return LISA_VFTINTRM_WU_S;
            case 0x99:
                /* 01110010 10011110 011001.. ........ */
                /* la.decode:1735 */
                return LISA_VFTINTRM_LU_D;
            case 0x9a:
                /* 01110010 10011110 011010.. ........ */
                /* la.decode:1736 */
                return LISA_VFTINTRP_WU_S;
            case 0x9b:
                /* 01110010 10011110 011011.. ........ */
                /* la.decode:1737 */
                return LISA_VFTINTRP_LU_D;
            case 0x9c:
                /* 01110010 10011110 011100.. ........ */
                /* la.decode:1738 */
                return LISA_VFTINTRZ_WU_S;
            case 0x9d:
                /* 01110010 10011110 011101.. ........ */
                /* la.decode:1739 */
                return LISA_VFTINTRZ_LU_D;
            case 0x9e:
                /* 01110010 10011110 011110.. ........ */
                /* la.decode:1740 */
                return LISA_VFTINTRNE_WU_S;
            case 0x9f:
                /* 01110010 10011110 011111.. ........ */
                /* la.decode:1741 */
                return LISA_VFTINTRNE_LU_D;
            case 0xa0:
                /* 01110010 10011110 100000.. ........ */
                /* la.decode:1742 */
                return LISA_VFTINTL_L_S;
            case 0xa1:
                /* 01110010 10011110 100001.. ........ */
                /* la.decode:1743 */
                return LISA_VFTINTH_L_S;
            case 0xa2:
                /* 01110010 10011110 100010.. ........ */
                /* la.decode:1744 */
                return LISA_VFTINTRML_L_S;
            case 0xa3:
                /* 01110010 10011110 100011.. ........ */
                /* la.decode:1745 */
                return LISA_VFTINTRMH_L_S;
            case 0xa4:
                /* 01110010 10011110 100100.. ........ */
                /* la.decode:1746 */
                return LISA_VFTINTRPL_L_S;
            case 0xa5:
                /* 01110010 10011110 100101.. ........ */
                /* la.decode:1747 */
                return LISA_VFTINTRPH_L_S;
            case 0xa6:
                /* 01110010 10011110 100110.. ........ */
                /* la.decode:1748 */
                return LISA_VFTINTRZL_L_S;
            case 0xa7:
                /* 01110010 10011110 100111.. ........ */
                /* la.decode:1749 */
                return LISA_VFTINTRZH_L_S;
            case 0xa8:
                /* 01110010 10011110 101000.. ........ */
                /* la.decode:1750 */
                return LISA_VFTINTRNEL_L_S;
            case 0xa9:
                /* 01110010 10011110 101001.. ........ */
                /* la.decode:1751 */
                return LISA_VFTINTRNEH_L_S;
            case 0xaa:
                /* 01110010 10011110 101010.. ........ */
                /* la.decode:1752 */
                return LISA_VFTINTL_LU_S;
            case 0xab:
                /* 01110010 10011110 101011.. ........ */
                /* la.decode:1753 */
                return LISA_VFTINTH_LU_S;
            case 0xac:
                /* 01110010 10011110 101100.. ........ */
                /* la.decode:1754 */
                return LISA_VFTINTRML_LU_S;
            case 0xad:
                /* 01110010 10011110 101101.. ........ */
                /* la.decode:1755 */
                return LISA_VFTINTRMH_LU_S;
            case 0xae:
                /* 01110010 10011110 101110.. ........ */
                /* la.decode:1756 */
                return LISA_VFTINTRPL_LU_S;
            case 0xaf:
                /* 01110010 10011110 101111.. ........ */
                /* la.decode:1757 */
                return LISA_VFTINTRPH_LU_S;
            case 0xb0:
                /* 01110010 10011110 110000.. ........ */
                /* la.decode:1758 */
                return LISA_VFTINTRZL_LU_S;
            case 0xb1:
                /* 01110010 10011110 110001.. ........ */
                /* la.decode:1759 */
                return LISA_VFTINTRZH_LU_S;
            case 0xb2:
                /* 01110010 10011110 110010.. ........ */
                /* la.decode:1760 */
                return LISA_VFTINTRNEL_LU_S;
            case 0xb3:
                /* 01110010 10011110 110011.. ........ */
                /* la.decode:1761 */
                return LISA_VFTINTRNEH_LU_S;
            case 0xb8:
                /* 01110010 10011110 111000.. ........ */
                /* la.decode:1762 */
                return LISA_VEXTH_H_B;
            case 0xb9:
                /* 01110010 10011110 111001.. ........ */
                /* la.decode:1763 */
                return LISA_VEXTH_W_H;
            case 0xba:
                /* 01110010 10011110 111010.. ........ */
                /* la.decode:1764 */
                return LISA_VEXTH_D_W;
            case 0xbb:
                /* 01110010 10011110 111011.. ........ */
                /* la.decode:1765 */
                return LISA_VEXTH_Q_D;
            case 0xbc:
                /* 01110010 10011110 111100.. ........ */
                /* la.decode:1766 */
                return LISA_VEXTH_HU_BU;
            case 0xbd:
                /* 01110010 10011110 111101.. ........ */
                /* la.decode:1767 */
                return LISA_VEXTH_WU_HU;
            case 0xbe:
                /* 01110010 10011110 111110.. ........ */
                /* la.decode:1768 */
                return LISA_VEXTH_DU_WU;
            case 0xbf:
                /* 01110010 10011110 111111.. ........ */
                /* la.decode:1769 */
                return LISA_VEXTH_QU_DU;
            case 0xc0:
                /* 01110010 10011111 000000.. ........ */
                /* la.decode:1770 */
                return LISA_VREPLGR2VR_B;
            case 0xc1:
                /* 01110010 10011111 000001.. ........ */
                /* la.decode:1771 */
                return LISA_VREPLGR2VR_H;
            case 0xc2:
                /* 01110010 10011111 000010.. ........ */
                /* la.decode:1772 */
                return LISA_VREPLGR2VR_W;
            case 0xc3:
                /* 01110010 10011111 000011.. ........ */
                /* la.decode:1773 */
                return LISA_VREPLGR2VR_D;
            }
            return LISA_INVALID;
        case 0xa8:
            /* 01110010 101000.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110010 10100000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110010 10100000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110010 10100000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110010 10100000 001..... ........ */
                            /* la.decode:1774 */
                            return LISA_VROTRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110010 10100000 01...... ........ */
                        /* la.decode:1775 */
                        return LISA_VROTRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110010 10100000 1....... ........ */
                    /* la.decode:1776 */
                    return LISA_VROTRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110010 10100001 ........ ........ */
                /* la.decode:1777 */
                return LISA_VROTRI_D;
            }
            return LISA_INVALID;
        case 0xa9:
            /* 01110010 101001.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110010 10100100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110010 10100100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110010 10100100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110010 10100100 001..... ........ */
                            /* la.decode:1778 */
                            return LISA_VSRLRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110010 10100100 01...... ........ */
                        /* la.decode:1779 */
                        return LISA_VSRLRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110010 10100100 1....... ........ */
                    /* la.decode:1780 */
                    return LISA_VSRLRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110010 10100101 ........ ........ */
                /* la.decode:1781 */
                return LISA_VSRLRI_D;
            }
            return LISA_INVALID;
        case 0xaa:
            /* 01110010 101010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110010 10101000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110010 10101000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110010 10101000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110010 10101000 001..... ........ */
                            /* la.decode:1782 */
                            return LISA_VSRARI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110010 10101000 01...... ........ */
                        /* la.decode:1783 */
                        return LISA_VSRARI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110010 10101000 1....... ........ */
                    /* la.decode:1784 */
                    return LISA_VSRARI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110010 10101001 ........ ........ */
                /* la.decode:1785 */
                return LISA_VSRARI_D;
            }
            return LISA_INVALID;
        case 0xba:
            /* 01110010 111010.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110010 11101011 10...... ........ */
                /* la.decode:1786 */
                return LISA_VINSGR2VR_B;
            case 0xf:
                /* 01110010 11101011 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110010 11101011 110..... ........ */
                    /* la.decode:1787 */
                    return LISA_VINSGR2VR_H;
                case 0x1:
                    /* 01110010 11101011 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110010 11101011 1110.... ........ */
                        /* la.decode:1788 */
                        return LISA_VINSGR2VR_W;
                    case 0x1:
                        /* 01110010 11101011 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110010 11101011 11110... ........ */
                            /* la.decode:1789 */
                            return LISA_VINSGR2VR_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbb:
            /* 01110010 111011.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110010 11101111 10...... ........ */
                /* la.decode:1790 */
                return LISA_VPICKVE2GR_B;
            case 0xf:
                /* 01110010 11101111 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110010 11101111 110..... ........ */
                    /* la.decode:1791 */
                    return LISA_VPICKVE2GR_H;
                case 0x1:
                    /* 01110010 11101111 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110010 11101111 1110.... ........ */
                        /* la.decode:1792 */
                        return LISA_VPICKVE2GR_W;
                    case 0x1:
                        /* 01110010 11101111 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110010 11101111 11110... ........ */
                            /* la.decode:1793 */
                            return LISA_VPICKVE2GR_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbc:
            /* 01110010 111100.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110010 11110011 10...... ........ */
                /* la.decode:1794 */
                return LISA_VPICKVE2GR_BU;
            case 0xf:
                /* 01110010 11110011 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110010 11110011 110..... ........ */
                    /* la.decode:1795 */
                    return LISA_VPICKVE2GR_HU;
                case 0x1:
                    /* 01110010 11110011 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110010 11110011 1110.... ........ */
                        /* la.decode:1796 */
                        return LISA_VPICKVE2GR_WU;
                    case 0x1:
                        /* 01110010 11110011 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110010 11110011 11110... ........ */
                            /* la.decode:1797 */
                            return LISA_VPICKVE2GR_DU;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbd:
            /* 01110010 111101.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110010 11110111 10...... ........ */
                /* la.decode:1798 */
                return LISA_VREPLVEI_B;
            case 0xf:
                /* 01110010 11110111 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110010 11110111 110..... ........ */
                    /* la.decode:1799 */
                    return LISA_VREPLVEI_H;
                case 0x1:
                    /* 01110010 11110111 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110010 11110111 1110.... ........ */
                        /* la.decode:1800 */
                        return LISA_VREPLVEI_W;
                    case 0x1:
                        /* 01110010 11110111 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110010 11110111 11110... ........ */
                            /* la.decode:1801 */
                            return LISA_VREPLVEI_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbe:
            /* 01110010 111110.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110010 11111011 10...... ........ */
                /* la.decode:1802 */
                return LISA_VEXTRCOLI_B;
            case 0xf:
                /* 01110010 11111011 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110010 11111011 110..... ........ */
                    /* la.decode:1803 */
                    return LISA_VEXTRCOLI_H;
                case 0x1:
                    /* 01110010 11111011 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110010 11111011 1110.... ........ */
                        /* la.decode:1804 */
                        return LISA_VEXTRCOLI_W;
                    case 0x1:
                        /* 01110010 11111011 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110010 11111011 11110... ........ */
                            /* la.decode:1805 */
                            return LISA_VEXTRCOLI_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xc2:
            /* 01110011 000010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00001000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00001000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00001000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00001000 001..... ........ */
                            /* la.decode:1806 */
                            return LISA_VSLLWIL_H_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00001000 01...... ........ */
                        /* la.decode:1807 */
                        return LISA_VSLLWIL_W_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00001000 1....... ........ */
                    /* la.decode:1808 */
                    return LISA_VSLLWIL_D_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00001001 ........ ........ */
                if ((insn & 0x0000fc00) == 0x00000000) {
                    /* 01110011 00001001 000000.. ........ */
                    /* la.decode:1810 */
                    return LISA_VEXTL_Q_D;
                }
                /* la.decode:1811 */
                return LISA_VSLLWIL_Q_D;
            }
            return LISA_INVALID;
        case 0xc3:
            /* 01110011 000011.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00001100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00001100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00001100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00001100 001..... ........ */
                            /* la.decode:1813 */
                            return LISA_VSLLWIL_HU_BU;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00001100 01...... ........ */
                        /* la.decode:1814 */
                        return LISA_VSLLWIL_WU_HU;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00001100 1....... ........ */
                    /* la.decode:1815 */
                    return LISA_VSLLWIL_DU_WU;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00001101 ........ ........ */
                if ((insn & 0x0000fc00) == 0x00000000) {
                    /* 01110011 00001101 000000.. ........ */
                    /* la.decode:1817 */
                    return LISA_VEXTL_QU_DU;
                }
                /* la.decode:1818 */
                return LISA_VSLLWIL_QU_DU;
            }
            return LISA_INVALID;
        case 0xc4:
            /* 01110011 000100.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00010000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00010000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00010000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00010000 001..... ........ */
                            /* la.decode:1820 */
                            return LISA_VBITCLRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00010000 01...... ........ */
                        /* la.decode:1821 */
                        return LISA_VBITCLRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00010000 1....... ........ */
                    /* la.decode:1822 */
                    return LISA_VBITCLRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00010001 ........ ........ */
                /* la.decode:1823 */
                return LISA_VBITCLRI_D;
            }
            return LISA_INVALID;
        case 0xc5:
            /* 01110011 000101.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00010100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00010100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00010100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00010100 001..... ........ */
                            /* la.decode:1824 */
                            return LISA_VBITSETI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00010100 01...... ........ */
                        /* la.decode:1825 */
                        return LISA_VBITSETI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00010100 1....... ........ */
                    /* la.decode:1826 */
                    return LISA_VBITSETI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00010101 ........ ........ */
                /* la.decode:1827 */
                return LISA_VBITSETI_D;
            }
            return LISA_INVALID;
        case 0xc6:
            /* 01110011 000110.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00011000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00011000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00011000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00011000 001..... ........ */
                            /* la.decode:1828 */
                            return LISA_VBITREVI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00011000 01...... ........ */
                        /* la.decode:1829 */
                        return LISA_VBITREVI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00011000 1....... ........ */
                    /* la.decode:1830 */
                    return LISA_VBITREVI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00011001 ........ ........ */
                /* la.decode:1831 */
                return LISA_VBITREVI_D;
            }
            return LISA_INVALID;
        case 0xc7:
            /* 01110011 000111.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00011100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00011100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00011100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00011100 001..... ........ */
                            /* la.decode:1832 */
                            return LISA_VBSTRC12I_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00011100 01...... ........ */
                        /* la.decode:1833 */
                        return LISA_VBSTRC12I_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00011100 1....... ........ */
                    /* la.decode:1834 */
                    return LISA_VBSTRC12I_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00011101 ........ ........ */
                /* la.decode:1835 */
                return LISA_VBSTRC12I_D;
            }
            return LISA_INVALID;
        case 0xc8:
            /* 01110011 001000.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00100000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00100000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00100000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00100000 001..... ........ */
                            /* la.decode:1836 */
                            return LISA_VBSTRC21I_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00100000 01...... ........ */
                        /* la.decode:1837 */
                        return LISA_VBSTRC21I_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00100000 1....... ........ */
                    /* la.decode:1838 */
                    return LISA_VBSTRC21I_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00100001 ........ ........ */
                /* la.decode:1839 */
                return LISA_VBSTRC21I_D;
            }
            return LISA_INVALID;
        case 0xc9:
            /* 01110011 001001.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00100100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00100100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00100100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00100100 001..... ........ */
                            /* la.decode:1840 */
                            return LISA_VSAT_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00100100 01...... ........ */
                        /* la.decode:1841 */
                        return LISA_VSAT_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00100100 1....... ........ */
                    /* la.decode:1842 */
                    return LISA_VSAT_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00100101 ........ ........ */
                /* la.decode:1843 */
                return LISA_VSAT_D;
            }
            return LISA_INVALID;
        case 0xca:
            /* 01110011 001010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00101000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00101000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00101000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00101000 001..... ........ */
                            /* la.decode:1844 */
                            return LISA_VSAT_BU;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00101000 01...... ........ */
                        /* la.decode:1845 */
                        return LISA_VSAT_HU;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00101000 1....... ........ */
                    /* la.decode:1846 */
                    return LISA_VSAT_WU;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00101001 ........ ........ */
                /* la.decode:1847 */
                return LISA_VSAT_DU;
            }
            return LISA_INVALID;
        case 0xcb:
            /* 01110011 001011.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00101100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00101100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00101100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00101100 001..... ........ */
                            /* la.decode:1848 */
                            return LISA_VSLLI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00101100 01...... ........ */
                        /* la.decode:1849 */
                        return LISA_VSLLI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00101100 1....... ........ */
                    /* la.decode:1850 */
                    return LISA_VSLLI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00101101 ........ ........ */
                /* la.decode:1851 */
                return LISA_VSLLI_D;
            }
            return LISA_INVALID;
        case 0xcc:
            /* 01110011 001100.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00110000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00110000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00110000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00110000 001..... ........ */
                            /* la.decode:1852 */
                            return LISA_VSRLI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00110000 01...... ........ */
                        /* la.decode:1853 */
                        return LISA_VSRLI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00110000 1....... ........ */
                    /* la.decode:1854 */
                    return LISA_VSRLI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00110001 ........ ........ */
                /* la.decode:1855 */
                return LISA_VSRLI_D;
            }
            return LISA_INVALID;
        case 0xcd:
            /* 01110011 001101.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110011 00110100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110011 00110100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110011 00110100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110011 00110100 001..... ........ */
                            /* la.decode:1856 */
                            return LISA_VSRAI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00110100 01...... ........ */
                        /* la.decode:1857 */
                        return LISA_VSRAI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00110100 1....... ........ */
                    /* la.decode:1858 */
                    return LISA_VSRAI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 00110101 ........ ........ */
                /* la.decode:1859 */
                return LISA_VSRAI_D;
            }
            return LISA_INVALID;
        case 0xce:
            /* 01110011 001110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0011100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 00111000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 00111000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 00111000 01...... ........ */
                            /* la.decode:1860 */
                            return LISA_VSRLRNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00111000 1....... ........ */
                        /* la.decode:1861 */
                        return LISA_VSRLRNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00111001 ........ ........ */
                    /* la.decode:1862 */
                    return LISA_VSRLRNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0011101. ........ ........ */
                /* la.decode:1863 */
                return LISA_VSRLRNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xcf:
            /* 01110011 001111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0011110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 00111100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 00111100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 00111100 01...... ........ */
                            /* la.decode:1864 */
                            return LISA_VSRARNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 00111100 1....... ........ */
                        /* la.decode:1865 */
                        return LISA_VSRARNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 00111101 ........ ........ */
                    /* la.decode:1866 */
                    return LISA_VSRARNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0011111. ........ ........ */
                /* la.decode:1867 */
                return LISA_VSRARNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xd0:
            /* 01110011 010000.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0100000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01000000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01000000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01000000 01...... ........ */
                            /* la.decode:1868 */
                            return LISA_VSRLNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01000000 1....... ........ */
                        /* la.decode:1869 */
                        return LISA_VSRLNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01000001 ........ ........ */
                    /* la.decode:1870 */
                    return LISA_VSRLNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0100001. ........ ........ */
                /* la.decode:1871 */
                return LISA_VSRLNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd1:
            /* 01110011 010001.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0100010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01000100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01000100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01000100 01...... ........ */
                            /* la.decode:1872 */
                            return LISA_VSRLRNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01000100 1....... ........ */
                        /* la.decode:1873 */
                        return LISA_VSRLRNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01000101 ........ ........ */
                    /* la.decode:1874 */
                    return LISA_VSRLRNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0100011. ........ ........ */
                /* la.decode:1875 */
                return LISA_VSRLRNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd2:
            /* 01110011 010010.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0100100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01001000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01001000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01001000 01...... ........ */
                            /* la.decode:1876 */
                            return LISA_VSSRLNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01001000 1....... ........ */
                        /* la.decode:1877 */
                        return LISA_VSSRLNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01001001 ........ ........ */
                    /* la.decode:1878 */
                    return LISA_VSSRLNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0100101. ........ ........ */
                /* la.decode:1879 */
                return LISA_VSSRLNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd3:
            /* 01110011 010011.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0100110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01001100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01001100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01001100 01...... ........ */
                            /* la.decode:1880 */
                            return LISA_VSSRLNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01001100 1....... ........ */
                        /* la.decode:1881 */
                        return LISA_VSSRLNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01001101 ........ ........ */
                    /* la.decode:1882 */
                    return LISA_VSSRLNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0100111. ........ ........ */
                /* la.decode:1883 */
                return LISA_VSSRLNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xd4:
            /* 01110011 010100.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0101000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01010000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01010000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01010000 01...... ........ */
                            /* la.decode:1884 */
                            return LISA_VSSRLRNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01010000 1....... ........ */
                        /* la.decode:1885 */
                        return LISA_VSSRLRNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01010001 ........ ........ */
                    /* la.decode:1886 */
                    return LISA_VSSRLRNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0101001. ........ ........ */
                /* la.decode:1887 */
                return LISA_VSSRLRNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd5:
            /* 01110011 010101.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0101010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01010100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01010100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01010100 01...... ........ */
                            /* la.decode:1888 */
                            return LISA_VSSRLRNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01010100 1....... ........ */
                        /* la.decode:1889 */
                        return LISA_VSSRLRNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01010101 ........ ........ */
                    /* la.decode:1890 */
                    return LISA_VSSRLRNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0101011. ........ ........ */
                /* la.decode:1891 */
                return LISA_VSSRLRNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xd6:
            /* 01110011 010110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0101100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01011000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01011000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01011000 01...... ........ */
                            /* la.decode:1892 */
                            return LISA_VSRANI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01011000 1....... ........ */
                        /* la.decode:1893 */
                        return LISA_VSRANI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01011001 ........ ........ */
                    /* la.decode:1894 */
                    return LISA_VSRANI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0101101. ........ ........ */
                /* la.decode:1895 */
                return LISA_VSRANI_D_Q;
            }
            return LISA_INVALID;
        case 0xd7:
            /* 01110011 010111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0101110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01011100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01011100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01011100 01...... ........ */
                            /* la.decode:1896 */
                            return LISA_VSRARNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01011100 1....... ........ */
                        /* la.decode:1897 */
                        return LISA_VSRARNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01011101 ........ ........ */
                    /* la.decode:1898 */
                    return LISA_VSRARNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0101111. ........ ........ */
                /* la.decode:1899 */
                return LISA_VSRARNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd8:
            /* 01110011 011000.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0110000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01100000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01100000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01100000 01...... ........ */
                            /* la.decode:1900 */
                            return LISA_VSSRANI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01100000 1....... ........ */
                        /* la.decode:1901 */
                        return LISA_VSSRANI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01100001 ........ ........ */
                    /* la.decode:1902 */
                    return LISA_VSSRANI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0110001. ........ ........ */
                /* la.decode:1903 */
                return LISA_VSSRANI_D_Q;
            }
            return LISA_INVALID;
        case 0xd9:
            /* 01110011 011001.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0110010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01100100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01100100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01100100 01...... ........ */
                            /* la.decode:1904 */
                            return LISA_VSSRANI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01100100 1....... ........ */
                        /* la.decode:1905 */
                        return LISA_VSSRANI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01100101 ........ ........ */
                    /* la.decode:1906 */
                    return LISA_VSSRANI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0110011. ........ ........ */
                /* la.decode:1907 */
                return LISA_VSSRANI_DU_Q;
            }
            return LISA_INVALID;
        case 0xda:
            /* 01110011 011010.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0110100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01101000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01101000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01101000 01...... ........ */
                            /* la.decode:1908 */
                            return LISA_VSSRARNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01101000 1....... ........ */
                        /* la.decode:1909 */
                        return LISA_VSSRARNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01101001 ........ ........ */
                    /* la.decode:1910 */
                    return LISA_VSSRARNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0110101. ........ ........ */
                /* la.decode:1911 */
                return LISA_VSSRARNI_D_Q;
            }
            return LISA_INVALID;
        case 0xdb:
            /* 01110011 011011.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0110110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01101100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01101100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01101100 01...... ........ */
                            /* la.decode:1912 */
                            return LISA_VSSRARNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01101100 1....... ........ */
                        /* la.decode:1913 */
                        return LISA_VSSRARNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01101101 ........ ........ */
                    /* la.decode:1914 */
                    return LISA_VSSRARNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0110111. ........ ........ */
                /* la.decode:1915 */
                return LISA_VSSRARNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xdc:
            /* 01110011 011100.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0111000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01110000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01110000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01110000 01...... ........ */
                            /* la.decode:1916 */
                            return LISA_VSSRLRNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01110000 1....... ........ */
                        /* la.decode:1917 */
                        return LISA_VSSRLRNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01110001 ........ ........ */
                    /* la.decode:1918 */
                    return LISA_VSSRLRNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0111001. ........ ........ */
                /* la.decode:1919 */
                return LISA_VSSRLRNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xdd:
            /* 01110011 011101.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0111010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01110100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01110100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01110100 01...... ........ */
                            /* la.decode:1920 */
                            return LISA_VSSRLRNENI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01110100 1....... ........ */
                        /* la.decode:1921 */
                        return LISA_VSSRLRNENI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01110101 ........ ........ */
                    /* la.decode:1922 */
                    return LISA_VSSRLRNENI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0111011. ........ ........ */
                /* la.decode:1923 */
                return LISA_VSSRLRNENI_DU_Q;
            }
            return LISA_INVALID;
        case 0xde:
            /* 01110011 011110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0111100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01111000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01111000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01111000 01...... ........ */
                            /* la.decode:1924 */
                            return LISA_VSSRARNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01111000 1....... ........ */
                        /* la.decode:1925 */
                        return LISA_VSSRARNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01111001 ........ ........ */
                    /* la.decode:1926 */
                    return LISA_VSSRARNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0111101. ........ ........ */
                /* la.decode:1927 */
                return LISA_VSSRARNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xdf:
            /* 01110011 011111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110011 0111110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110011 01111100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110011 01111100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110011 01111100 01...... ........ */
                            /* la.decode:1928 */
                            return LISA_VSSRARNENI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110011 01111100 1....... ........ */
                        /* la.decode:1929 */
                        return LISA_VSSRARNENI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110011 01111101 ........ ........ */
                    /* la.decode:1930 */
                    return LISA_VSSRARNENI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110011 0111111. ........ ........ */
                /* la.decode:1931 */
                return LISA_VSSRARNENI_DU_Q;
            }
            return LISA_INVALID;
        case 0xe0:
            /* 01110011 100000.. ........ ........ */
            /* la.decode:1932 */
            return LISA_VEXTRINS_D;
        case 0xe1:
            /* 01110011 100001.. ........ ........ */
            /* la.decode:1933 */
            return LISA_VEXTRINS_W;
        case 0xe2:
            /* 01110011 100010.. ........ ........ */
            /* la.decode:1934 */
            return LISA_VEXTRINS_H;
        case 0xe3:
            /* 01110011 100011.. ........ ........ */
            /* la.decode:1935 */
            return LISA_VEXTRINS_B;
        case 0xe4:
            /* 01110011 100100.. ........ ........ */
            /* la.decode:1936 */
            return LISA_VSHUF4I_B;
        case 0xe5:
            /* 01110011 100101.. ........ ........ */
            /* la.decode:1937 */
            return LISA_VSHUF4I_H;
        case 0xe6:
            /* 01110011 100110.. ........ ........ */
            /* la.decode:1938 */
            return LISA_VSHUF4I_W;
        case 0xe7:
            /* 01110011 100111.. ........ ........ */
            /* la.decode:1939 */
            return LISA_VSHUF4I_D;
        case 0xe8:
            /* 01110011 101000.. ........ ........ */
            /* la.decode:1940 */
            return LISA_VSHUFI1_B;
        case 0xe9:
            /* 01110011 101001.. ........ ........ */
            /* la.decode:1941 */
            return LISA_VSHUFI2_B;
        case 0xea:
            /* 01110011 101010.. ........ ........ */
            /* la.decode:1942 */
            return LISA_VSHUFI3_B;
        case 0xeb:
            /* 01110011 101011.. ........ ........ */
            /* la.decode:1943 */
            return LISA_VSHUFI4_B;
        case 0xec:
            /* 01110011 101100.. ........ ........ */
            /* la.decode:1944 */
            return LISA_VSHUFI1_H;
        case 0xed:
            /* 01110011 101101.. ........ ........ */
            /* la.decode:1945 */
            return LISA_VSHUFI2_H;
        case 0xee:
            /* 01110011 101110.. ........ ........ */
            /* la.decode:1946 */
            return LISA_VSELI_H;
        case 0xef:
            /* 01110011 101111.. ........ ........ */
            /* la.decode:1947 */
            return LISA_VSELI_W;
        case 0xf0:
            /* 01110011 110000.. ........ ........ */
            /* la.decode:1948 */
            return LISA_VSELI_D;
        case 0xf1:
            /* 01110011 110001.. ........ ........ */
            /* la.decode:1949 */
            return LISA_VBITSELI_B;
        case 0xf2:
            /* 01110011 110010.. ........ ........ */
            /* la.decode:1950 */
            return LISA_VBITMVZI_B;
        case 0xf3:
            /* 01110011 110011.. ........ ........ */
            /* la.decode:1951 */
            return LISA_VBITMVNZI_B;
        case 0xf4:
            /* 01110011 110100.. ........ ........ */
            /* la.decode:1952 */
            return LISA_VANDI_B;
        case 0xf5:
            /* 01110011 110101.. ........ ........ */
            /* la.decode:1953 */
            return LISA_VORI_B;
        case 0xf6:
            /* 01110011 110110.. ........ ........ */
            /* la.decode:1954 */
            return LISA_VXORI_B;
        case 0xf7:
            /* 01110011 110111.. ........ ........ */
            /* la.decode:1955 */
            return LISA_VNORI_B;
        case 0xf8:
            /* 01110011 111000.. ........ ........ */
            /* la.decode:1956 */
            return LISA_VLDI;
        case 0xf9:
            /* 01110011 111001.. ........ ........ */
            /* la.decode:1957 */
            return LISA_VPERMI_W;
        }
        return LISA_INVALID;
    case 0x1d:
        /* 011101.. ........ ........ ........ */
        switch ((insn >> 18) & 0xff) {
        case 0x0:
            /* 01110100 000000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00000000 0....... ........ */
                /* la.decode:1958 */
                return LISA_XVSEQ_B;
            case 0x1:
                /* 01110100 00000000 1....... ........ */
                /* la.decode:1959 */
                return LISA_XVSEQ_H;
            case 0x2:
                /* 01110100 00000001 0....... ........ */
                /* la.decode:1960 */
                return LISA_XVSEQ_W;
            case 0x3:
                /* 01110100 00000001 1....... ........ */
                /* la.decode:1961 */
                return LISA_XVSEQ_D;
            case 0x4:
                /* 01110100 00000010 0....... ........ */
                /* la.decode:1962 */
                return LISA_XVSLE_B;
            case 0x5:
                /* 01110100 00000010 1....... ........ */
                /* la.decode:1963 */
                return LISA_XVSLE_H;
            case 0x6:
                /* 01110100 00000011 0....... ........ */
                /* la.decode:1964 */
                return LISA_XVSLE_W;
            case 0x7:
                /* 01110100 00000011 1....... ........ */
                /* la.decode:1965 */
                return LISA_XVSLE_D;
            }
            return LISA_INVALID;
        case 0x1:
            /* 01110100 000001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00000100 0....... ........ */
                /* la.decode:1966 */
                return LISA_XVSLE_BU;
            case 0x1:
                /* 01110100 00000100 1....... ........ */
                /* la.decode:1967 */
                return LISA_XVSLE_HU;
            case 0x2:
                /* 01110100 00000101 0....... ........ */
                /* la.decode:1968 */
                return LISA_XVSLE_WU;
            case 0x3:
                /* 01110100 00000101 1....... ........ */
                /* la.decode:1969 */
                return LISA_XVSLE_DU;
            case 0x4:
                /* 01110100 00000110 0....... ........ */
                /* la.decode:1970 */
                return LISA_XVSLT_B;
            case 0x5:
                /* 01110100 00000110 1....... ........ */
                /* la.decode:1971 */
                return LISA_XVSLT_H;
            case 0x6:
                /* 01110100 00000111 0....... ........ */
                /* la.decode:1972 */
                return LISA_XVSLT_W;
            case 0x7:
                /* 01110100 00000111 1....... ........ */
                /* la.decode:1973 */
                return LISA_XVSLT_D;
            }
            return LISA_INVALID;
        case 0x2:
            /* 01110100 000010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00001000 0....... ........ */
                /* la.decode:1974 */
                return LISA_XVSLT_BU;
            case 0x1:
                /* 01110100 00001000 1....... ........ */
                /* la.decode:1975 */
                return LISA_XVSLT_HU;
            case 0x2:
                /* 01110100 00001001 0....... ........ */
                /* la.decode:1976 */
                return LISA_XVSLT_WU;
            case 0x3:
                /* 01110100 00001001 1....... ........ */
                /* la.decode:1977 */
                return LISA_XVSLT_DU;
            case 0x4:
                /* 01110100 00001010 0....... ........ */
                /* la.decode:1978 */
                return LISA_XVADD_B;
            case 0x5:
                /* 01110100 00001010 1....... ........ */
                /* la.decode:1979 */
                return LISA_XVADD_H;
            case 0x6:
                /* 01110100 00001011 0....... ........ */
                /* la.decode:1980 */
                return LISA_XVADD_W;
            case 0x7:
                /* 01110100 00001011 1....... ........ */
                /* la.decode:1981 */
                return LISA_XVADD_D;
            }
            return LISA_INVALID;
        case 0x3:
            /* 01110100 000011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00001100 0....... ........ */
                /* la.decode:1982 */
                return LISA_XVSUB_B;
            case 0x1:
                /* 01110100 00001100 1....... ........ */
                /* la.decode:1983 */
                return LISA_XVSUB_H;
            case 0x2:
                /* 01110100 00001101 0....... ........ */
                /* la.decode:1984 */
                return LISA_XVSUB_W;
            case 0x3:
                /* 01110100 00001101 1....... ........ */
                /* la.decode:1985 */
                return LISA_XVSUB_D;
            case 0x4:
                /* 01110100 00001110 0....... ........ */
                /* la.decode:1986 */
                return LISA_XVADDW_H_H_B;
            case 0x5:
                /* 01110100 00001110 1....... ........ */
                /* la.decode:1987 */
                return LISA_XVADDW_W_W_H;
            case 0x6:
                /* 01110100 00001111 0....... ........ */
                /* la.decode:1988 */
                return LISA_XVADDW_D_D_W;
            case 0x7:
                /* 01110100 00001111 1....... ........ */
                /* la.decode:1989 */
                return LISA_XVADDW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x4:
            /* 01110100 000100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00010000 0....... ........ */
                /* la.decode:1990 */
                return LISA_XVADDW_H_H_BU;
            case 0x1:
                /* 01110100 00010000 1....... ........ */
                /* la.decode:1991 */
                return LISA_XVADDW_W_W_HU;
            case 0x2:
                /* 01110100 00010001 0....... ........ */
                /* la.decode:1992 */
                return LISA_XVADDW_D_D_WU;
            case 0x3:
                /* 01110100 00010001 1....... ........ */
                /* la.decode:1993 */
                return LISA_XVADDW_Q_Q_DU;
            case 0x4:
                /* 01110100 00010010 0....... ........ */
                /* la.decode:1994 */
                return LISA_XVSUBW_H_H_B;
            case 0x5:
                /* 01110100 00010010 1....... ........ */
                /* la.decode:1995 */
                return LISA_XVSUBW_W_W_H;
            case 0x6:
                /* 01110100 00010011 0....... ........ */
                /* la.decode:1996 */
                return LISA_XVSUBW_D_D_W;
            case 0x7:
                /* 01110100 00010011 1....... ........ */
                /* la.decode:1997 */
                return LISA_XVSUBW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x5:
            /* 01110100 000101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00010100 0....... ........ */
                /* la.decode:1998 */
                return LISA_XVSUBW_H_H_BU;
            case 0x1:
                /* 01110100 00010100 1....... ........ */
                /* la.decode:1999 */
                return LISA_XVSUBW_W_W_HU;
            case 0x2:
                /* 01110100 00010101 0....... ........ */
                /* la.decode:2000 */
                return LISA_XVSUBW_D_D_WU;
            case 0x3:
                /* 01110100 00010101 1....... ........ */
                /* la.decode:2001 */
                return LISA_XVSUBW_Q_Q_DU;
            case 0x4:
                /* 01110100 00010110 0....... ........ */
                /* la.decode:2002 */
                return LISA_XVSADDW_H_H_B;
            case 0x5:
                /* 01110100 00010110 1....... ........ */
                /* la.decode:2003 */
                return LISA_XVSADDW_W_W_H;
            case 0x6:
                /* 01110100 00010111 0....... ........ */
                /* la.decode:2004 */
                return LISA_XVSADDW_D_D_W;
            case 0x7:
                /* 01110100 00010111 1....... ........ */
                /* la.decode:2005 */
                return LISA_XVSADDW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x6:
            /* 01110100 000110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00011000 0....... ........ */
                /* la.decode:2006 */
                return LISA_XVSADDW_HU_HU_BU;
            case 0x1:
                /* 01110100 00011000 1....... ........ */
                /* la.decode:2007 */
                return LISA_XVSADDW_WU_WU_HU;
            case 0x2:
                /* 01110100 00011001 0....... ........ */
                /* la.decode:2008 */
                return LISA_XVSADDW_DU_DU_WU;
            case 0x3:
                /* 01110100 00011001 1....... ........ */
                /* la.decode:2009 */
                return LISA_XVSADDW_QU_QU_DU;
            case 0x4:
                /* 01110100 00011010 0....... ........ */
                /* la.decode:2010 */
                return LISA_XVSSUBW_H_H_B;
            case 0x5:
                /* 01110100 00011010 1....... ........ */
                /* la.decode:2011 */
                return LISA_XVSSUBW_W_W_H;
            case 0x6:
                /* 01110100 00011011 0....... ........ */
                /* la.decode:2012 */
                return LISA_XVSSUBW_D_D_W;
            case 0x7:
                /* 01110100 00011011 1....... ........ */
                /* la.decode:2013 */
                return LISA_XVSSUBW_Q_Q_D;
            }
            return LISA_INVALID;
        case 0x7:
            /* 01110100 000111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00011100 0....... ........ */
                /* la.decode:2014 */
                return LISA_XVSSUBW_HU_HU_BU;
            case 0x1:
                /* 01110100 00011100 1....... ........ */
                /* la.decode:2015 */
                return LISA_XVSSUBW_WU_WU_HU;
            case 0x2:
                /* 01110100 00011101 0....... ........ */
                /* la.decode:2016 */
                return LISA_XVSSUBW_DU_DU_WU;
            case 0x3:
                /* 01110100 00011101 1....... ........ */
                /* la.decode:2017 */
                return LISA_XVSSUBW_QU_QU_DU;
            case 0x4:
                /* 01110100 00011110 0....... ........ */
                /* la.decode:2018 */
                return LISA_XVADDWEV_H_B;
            case 0x5:
                /* 01110100 00011110 1....... ........ */
                /* la.decode:2019 */
                return LISA_XVADDWEV_W_H;
            case 0x6:
                /* 01110100 00011111 0....... ........ */
                /* la.decode:2020 */
                return LISA_XVADDWEV_D_W;
            case 0x7:
                /* 01110100 00011111 1....... ........ */
                /* la.decode:2021 */
                return LISA_XVADDWEV_Q_D;
            }
            return LISA_INVALID;
        case 0x8:
            /* 01110100 001000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00100000 0....... ........ */
                /* la.decode:2022 */
                return LISA_XVSUBWEV_H_B;
            case 0x1:
                /* 01110100 00100000 1....... ........ */
                /* la.decode:2023 */
                return LISA_XVSUBWEV_W_H;
            case 0x2:
                /* 01110100 00100001 0....... ........ */
                /* la.decode:2024 */
                return LISA_XVSUBWEV_D_W;
            case 0x3:
                /* 01110100 00100001 1....... ........ */
                /* la.decode:2025 */
                return LISA_XVSUBWEV_Q_D;
            case 0x4:
                /* 01110100 00100010 0....... ........ */
                /* la.decode:2026 */
                return LISA_XVADDWOD_H_B;
            case 0x5:
                /* 01110100 00100010 1....... ........ */
                /* la.decode:2027 */
                return LISA_XVADDWOD_W_H;
            case 0x6:
                /* 01110100 00100011 0....... ........ */
                /* la.decode:2028 */
                return LISA_XVADDWOD_D_W;
            case 0x7:
                /* 01110100 00100011 1....... ........ */
                /* la.decode:2029 */
                return LISA_XVADDWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x9:
            /* 01110100 001001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00100100 0....... ........ */
                /* la.decode:2030 */
                return LISA_XVSUBWOD_H_B;
            case 0x1:
                /* 01110100 00100100 1....... ........ */
                /* la.decode:2031 */
                return LISA_XVSUBWOD_W_H;
            case 0x2:
                /* 01110100 00100101 0....... ........ */
                /* la.decode:2032 */
                return LISA_XVSUBWOD_D_W;
            case 0x3:
                /* 01110100 00100101 1....... ........ */
                /* la.decode:2033 */
                return LISA_XVSUBWOD_Q_D;
            case 0x4:
                /* 01110100 00100110 0....... ........ */
                /* la.decode:2034 */
                return LISA_XVADDWL_H_B;
            case 0x5:
                /* 01110100 00100110 1....... ........ */
                /* la.decode:2035 */
                return LISA_XVADDWL_W_H;
            case 0x6:
                /* 01110100 00100111 0....... ........ */
                /* la.decode:2036 */
                return LISA_XVADDWL_D_W;
            case 0x7:
                /* 01110100 00100111 1....... ........ */
                /* la.decode:2037 */
                return LISA_XVADDWL_Q_D;
            }
            return LISA_INVALID;
        case 0xa:
            /* 01110100 001010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00101000 0....... ........ */
                /* la.decode:2038 */
                return LISA_XVSUBWL_H_B;
            case 0x1:
                /* 01110100 00101000 1....... ........ */
                /* la.decode:2039 */
                return LISA_XVSUBWL_W_H;
            case 0x2:
                /* 01110100 00101001 0....... ........ */
                /* la.decode:2040 */
                return LISA_XVSUBWL_D_W;
            case 0x3:
                /* 01110100 00101001 1....... ........ */
                /* la.decode:2041 */
                return LISA_XVSUBWL_Q_D;
            case 0x4:
                /* 01110100 00101010 0....... ........ */
                /* la.decode:2042 */
                return LISA_XVADDWH_H_B;
            case 0x5:
                /* 01110100 00101010 1....... ........ */
                /* la.decode:2043 */
                return LISA_XVADDWH_W_H;
            case 0x6:
                /* 01110100 00101011 0....... ........ */
                /* la.decode:2044 */
                return LISA_XVADDWH_D_W;
            case 0x7:
                /* 01110100 00101011 1....... ........ */
                /* la.decode:2045 */
                return LISA_XVADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0xb:
            /* 01110100 001011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00101100 0....... ........ */
                /* la.decode:2046 */
                return LISA_XVSUBWH_H_B;
            case 0x1:
                /* 01110100 00101100 1....... ........ */
                /* la.decode:2047 */
                return LISA_XVSUBWH_W_H;
            case 0x2:
                /* 01110100 00101101 0....... ........ */
                /* la.decode:2048 */
                return LISA_XVSUBWH_D_W;
            case 0x3:
                /* 01110100 00101101 1....... ........ */
                /* la.decode:2049 */
                return LISA_XVSUBWH_Q_D;
            case 0x4:
                /* 01110100 00101110 0....... ........ */
                /* la.decode:2050 */
                return LISA_XVADDWEV_H_BU;
            case 0x5:
                /* 01110100 00101110 1....... ........ */
                /* la.decode:2051 */
                return LISA_XVADDWEV_W_HU;
            case 0x6:
                /* 01110100 00101111 0....... ........ */
                /* la.decode:2052 */
                return LISA_XVADDWEV_D_WU;
            case 0x7:
                /* 01110100 00101111 1....... ........ */
                /* la.decode:2053 */
                return LISA_XVADDWEV_Q_DU;
            }
            return LISA_INVALID;
        case 0xc:
            /* 01110100 001100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00110000 0....... ........ */
                /* la.decode:2054 */
                return LISA_XVSUBWEV_H_BU;
            case 0x1:
                /* 01110100 00110000 1....... ........ */
                /* la.decode:2055 */
                return LISA_XVSUBWEV_W_HU;
            case 0x2:
                /* 01110100 00110001 0....... ........ */
                /* la.decode:2056 */
                return LISA_XVSUBWEV_D_WU;
            case 0x3:
                /* 01110100 00110001 1....... ........ */
                /* la.decode:2057 */
                return LISA_XVSUBWEV_Q_DU;
            case 0x4:
                /* 01110100 00110010 0....... ........ */
                /* la.decode:2058 */
                return LISA_XVADDWOD_H_BU;
            case 0x5:
                /* 01110100 00110010 1....... ........ */
                /* la.decode:2059 */
                return LISA_XVADDWOD_W_HU;
            case 0x6:
                /* 01110100 00110011 0....... ........ */
                /* la.decode:2060 */
                return LISA_XVADDWOD_D_WU;
            case 0x7:
                /* 01110100 00110011 1....... ........ */
                /* la.decode:2061 */
                return LISA_XVADDWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0xd:
            /* 01110100 001101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00110100 0....... ........ */
                /* la.decode:2062 */
                return LISA_XVSUBWOD_H_BU;
            case 0x1:
                /* 01110100 00110100 1....... ........ */
                /* la.decode:2063 */
                return LISA_XVSUBWOD_W_HU;
            case 0x2:
                /* 01110100 00110101 0....... ........ */
                /* la.decode:2064 */
                return LISA_XVSUBWOD_D_WU;
            case 0x3:
                /* 01110100 00110101 1....... ........ */
                /* la.decode:2065 */
                return LISA_XVSUBWOD_Q_DU;
            case 0x4:
                /* 01110100 00110110 0....... ........ */
                /* la.decode:2066 */
                return LISA_XVADDWL_H_BU;
            case 0x5:
                /* 01110100 00110110 1....... ........ */
                /* la.decode:2067 */
                return LISA_XVADDWL_W_HU;
            case 0x6:
                /* 01110100 00110111 0....... ........ */
                /* la.decode:2068 */
                return LISA_XVADDWL_D_WU;
            case 0x7:
                /* 01110100 00110111 1....... ........ */
                /* la.decode:2069 */
                return LISA_XVADDWL_Q_DU;
            }
            return LISA_INVALID;
        case 0xe:
            /* 01110100 001110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00111000 0....... ........ */
                /* la.decode:2070 */
                return LISA_XVSUBWL_H_BU;
            case 0x1:
                /* 01110100 00111000 1....... ........ */
                /* la.decode:2071 */
                return LISA_XVSUBWL_W_HU;
            case 0x2:
                /* 01110100 00111001 0....... ........ */
                /* la.decode:2072 */
                return LISA_XVSUBWL_D_WU;
            case 0x3:
                /* 01110100 00111001 1....... ........ */
                /* la.decode:2073 */
                return LISA_XVSUBWL_Q_DU;
            case 0x4:
                /* 01110100 00111010 0....... ........ */
                /* la.decode:2074 */
                return LISA_XVADDWH_H_BU;
            case 0x5:
                /* 01110100 00111010 1....... ........ */
                /* la.decode:2075 */
                return LISA_XVADDWH_W_HU;
            case 0x6:
                /* 01110100 00111011 0....... ........ */
                /* la.decode:2076 */
                return LISA_XVADDWH_D_WU;
            case 0x7:
                /* 01110100 00111011 1....... ........ */
                /* la.decode:2077 */
                return LISA_XVADDWH_Q_DU;
            }
            return LISA_INVALID;
        case 0xf:
            /* 01110100 001111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 00111100 0....... ........ */
                /* la.decode:2078 */
                return LISA_XVSUBWH_H_BU;
            case 0x1:
                /* 01110100 00111100 1....... ........ */
                /* la.decode:2079 */
                return LISA_XVSUBWH_W_HU;
            case 0x2:
                /* 01110100 00111101 0....... ........ */
                /* la.decode:2080 */
                return LISA_XVSUBWH_D_WU;
            case 0x3:
                /* 01110100 00111101 1....... ........ */
                /* la.decode:2081 */
                return LISA_XVSUBWH_Q_DU;
            case 0x4:
                /* 01110100 00111110 0....... ........ */
                /* la.decode:2082 */
                return LISA_XVADDWEV_H_BU_B;
            case 0x5:
                /* 01110100 00111110 1....... ........ */
                /* la.decode:2083 */
                return LISA_XVADDWEV_W_HU_H;
            case 0x6:
                /* 01110100 00111111 0....... ........ */
                /* la.decode:2084 */
                return LISA_XVADDWEV_D_WU_W;
            case 0x7:
                /* 01110100 00111111 1....... ........ */
                /* la.decode:2085 */
                return LISA_XVADDWEV_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x10:
            /* 01110100 010000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01000000 0....... ........ */
                /* la.decode:2086 */
                return LISA_XVADDWOD_H_BU_B;
            case 0x1:
                /* 01110100 01000000 1....... ........ */
                /* la.decode:2087 */
                return LISA_XVADDWOD_W_HU_H;
            case 0x2:
                /* 01110100 01000001 0....... ........ */
                /* la.decode:2088 */
                return LISA_XVADDWOD_D_WU_W;
            case 0x3:
                /* 01110100 01000001 1....... ........ */
                /* la.decode:2089 */
                return LISA_XVADDWOD_Q_DU_D;
            case 0x4:
                /* 01110100 01000010 0....... ........ */
                /* la.decode:2090 */
                return LISA_XVADDWL_H_BU_B;
            case 0x5:
                /* 01110100 01000010 1....... ........ */
                /* la.decode:2091 */
                return LISA_XVADDWL_W_HU_H;
            case 0x6:
                /* 01110100 01000011 0....... ........ */
                /* la.decode:2092 */
                return LISA_XVADDWL_D_WU_W;
            case 0x7:
                /* 01110100 01000011 1....... ........ */
                /* la.decode:2093 */
                return LISA_XVADDWL_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x11:
            /* 01110100 010001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01000100 0....... ........ */
                /* la.decode:2094 */
                return LISA_XVADDWH_H_BU_B;
            case 0x1:
                /* 01110100 01000100 1....... ........ */
                /* la.decode:2095 */
                return LISA_XVADDWH_W_HU_H;
            case 0x2:
                /* 01110100 01000101 0....... ........ */
                /* la.decode:2096 */
                return LISA_XVADDWH_D_WU_W;
            case 0x3:
                /* 01110100 01000101 1....... ........ */
                /* la.decode:2097 */
                return LISA_XVADDWH_Q_DU_D;
            case 0x4:
                /* 01110100 01000110 0....... ........ */
                /* la.decode:2098 */
                return LISA_XVSADD_B;
            case 0x5:
                /* 01110100 01000110 1....... ........ */
                /* la.decode:2099 */
                return LISA_XVSADD_H;
            case 0x6:
                /* 01110100 01000111 0....... ........ */
                /* la.decode:2100 */
                return LISA_XVSADD_W;
            case 0x7:
                /* 01110100 01000111 1....... ........ */
                /* la.decode:2101 */
                return LISA_XVSADD_D;
            }
            return LISA_INVALID;
        case 0x12:
            /* 01110100 010010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01001000 0....... ........ */
                /* la.decode:2102 */
                return LISA_XVSSUB_B;
            case 0x1:
                /* 01110100 01001000 1....... ........ */
                /* la.decode:2103 */
                return LISA_XVSSUB_H;
            case 0x2:
                /* 01110100 01001001 0....... ........ */
                /* la.decode:2104 */
                return LISA_XVSSUB_W;
            case 0x3:
                /* 01110100 01001001 1....... ........ */
                /* la.decode:2105 */
                return LISA_XVSSUB_D;
            case 0x4:
                /* 01110100 01001010 0....... ........ */
                /* la.decode:2106 */
                return LISA_XVSADD_BU;
            case 0x5:
                /* 01110100 01001010 1....... ........ */
                /* la.decode:2107 */
                return LISA_XVSADD_HU;
            case 0x6:
                /* 01110100 01001011 0....... ........ */
                /* la.decode:2108 */
                return LISA_XVSADD_WU;
            case 0x7:
                /* 01110100 01001011 1....... ........ */
                /* la.decode:2109 */
                return LISA_XVSADD_DU;
            }
            return LISA_INVALID;
        case 0x13:
            /* 01110100 010011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01001100 0....... ........ */
                /* la.decode:2110 */
                return LISA_XVSSUB_BU;
            case 0x1:
                /* 01110100 01001100 1....... ........ */
                /* la.decode:2111 */
                return LISA_XVSSUB_HU;
            case 0x2:
                /* 01110100 01001101 0....... ........ */
                /* la.decode:2112 */
                return LISA_XVSSUB_WU;
            case 0x3:
                /* 01110100 01001101 1....... ........ */
                /* la.decode:2113 */
                return LISA_XVSSUB_DU;
            case 0x4:
                /* 01110100 01001110 0....... ........ */
                /* la.decode:2114 */
                return LISA_XVSSUB_BU_BU_B;
            case 0x5:
                /* 01110100 01001110 1....... ........ */
                /* la.decode:2115 */
                return LISA_XVSSUB_HU_HU_H;
            case 0x6:
                /* 01110100 01001111 0....... ........ */
                /* la.decode:2116 */
                return LISA_XVSSUB_WU_WU_W;
            case 0x7:
                /* 01110100 01001111 1....... ........ */
                /* la.decode:2117 */
                return LISA_XVSSUB_DU_DU_D;
            }
            return LISA_INVALID;
        case 0x14:
            /* 01110100 010100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01010000 0....... ........ */
                /* la.decode:2118 */
                return LISA_XVSSUB_BU_B_BU;
            case 0x1:
                /* 01110100 01010000 1....... ........ */
                /* la.decode:2119 */
                return LISA_XVSSUB_HU_H_HU;
            case 0x2:
                /* 01110100 01010001 0....... ........ */
                /* la.decode:2120 */
                return LISA_XVSSUB_WU_W_WU;
            case 0x3:
                /* 01110100 01010001 1....... ........ */
                /* la.decode:2121 */
                return LISA_XVSSUB_DU_D_DU;
            case 0x4:
                /* 01110100 01010010 0....... ........ */
                /* la.decode:2122 */
                return LISA_XVSSUB_B_BU_BU;
            case 0x5:
                /* 01110100 01010010 1....... ........ */
                /* la.decode:2123 */
                return LISA_XVSSUB_H_HU_HU;
            case 0x6:
                /* 01110100 01010011 0....... ........ */
                /* la.decode:2124 */
                return LISA_XVSSUB_W_WU_WU;
            case 0x7:
                /* 01110100 01010011 1....... ........ */
                /* la.decode:2125 */
                return LISA_XVSSUB_D_DU_DU;
            }
            return LISA_INVALID;
        case 0x15:
            /* 01110100 010101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01010100 0....... ........ */
                /* la.decode:2126 */
                return LISA_XVHADDW_H_B;
            case 0x1:
                /* 01110100 01010100 1....... ........ */
                /* la.decode:2127 */
                return LISA_XVHADDW_W_H;
            case 0x2:
                /* 01110100 01010101 0....... ........ */
                /* la.decode:2128 */
                return LISA_XVHADDW_D_W;
            case 0x3:
                /* 01110100 01010101 1....... ........ */
                /* la.decode:2129 */
                return LISA_XVHADDW_Q_D;
            case 0x4:
                /* 01110100 01010110 0....... ........ */
                /* la.decode:2130 */
                return LISA_XVHSUBW_H_B;
            case 0x5:
                /* 01110100 01010110 1....... ........ */
                /* la.decode:2131 */
                return LISA_XVHSUBW_W_H;
            case 0x6:
                /* 01110100 01010111 0....... ........ */
                /* la.decode:2132 */
                return LISA_XVHSUBW_D_W;
            case 0x7:
                /* 01110100 01010111 1....... ........ */
                /* la.decode:2133 */
                return LISA_XVHSUBW_Q_D;
            }
            return LISA_INVALID;
        case 0x16:
            /* 01110100 010110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01011000 0....... ........ */
                /* la.decode:2134 */
                return LISA_XVHADDW_HU_BU;
            case 0x1:
                /* 01110100 01011000 1....... ........ */
                /* la.decode:2135 */
                return LISA_XVHADDW_WU_HU;
            case 0x2:
                /* 01110100 01011001 0....... ........ */
                /* la.decode:2136 */
                return LISA_XVHADDW_DU_WU;
            case 0x3:
                /* 01110100 01011001 1....... ........ */
                /* la.decode:2137 */
                return LISA_XVHADDW_QU_DU;
            case 0x4:
                /* 01110100 01011010 0....... ........ */
                /* la.decode:2138 */
                return LISA_XVHSUBW_HU_BU;
            case 0x5:
                /* 01110100 01011010 1....... ........ */
                /* la.decode:2139 */
                return LISA_XVHSUBW_WU_HU;
            case 0x6:
                /* 01110100 01011011 0....... ........ */
                /* la.decode:2140 */
                return LISA_XVHSUBW_DU_WU;
            case 0x7:
                /* 01110100 01011011 1....... ........ */
                /* la.decode:2141 */
                return LISA_XVHSUBW_QU_DU;
            }
            return LISA_INVALID;
        case 0x17:
            /* 01110100 010111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01011100 0....... ........ */
                /* la.decode:2142 */
                return LISA_XVADDA_B;
            case 0x1:
                /* 01110100 01011100 1....... ........ */
                /* la.decode:2143 */
                return LISA_XVADDA_H;
            case 0x2:
                /* 01110100 01011101 0....... ........ */
                /* la.decode:2144 */
                return LISA_XVADDA_W;
            case 0x3:
                /* 01110100 01011101 1....... ........ */
                /* la.decode:2145 */
                return LISA_XVADDA_D;
            case 0x4:
                /* 01110100 01011110 0....... ........ */
                /* la.decode:2146 */
                return LISA_XVSADDA_B;
            case 0x5:
                /* 01110100 01011110 1....... ........ */
                /* la.decode:2147 */
                return LISA_XVSADDA_H;
            case 0x6:
                /* 01110100 01011111 0....... ........ */
                /* la.decode:2148 */
                return LISA_XVSADDA_W;
            case 0x7:
                /* 01110100 01011111 1....... ........ */
                /* la.decode:2149 */
                return LISA_XVSADDA_D;
            }
            return LISA_INVALID;
        case 0x18:
            /* 01110100 011000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01100000 0....... ........ */
                /* la.decode:2150 */
                return LISA_XVABSD_B;
            case 0x1:
                /* 01110100 01100000 1....... ........ */
                /* la.decode:2151 */
                return LISA_XVABSD_H;
            case 0x2:
                /* 01110100 01100001 0....... ........ */
                /* la.decode:2152 */
                return LISA_XVABSD_W;
            case 0x3:
                /* 01110100 01100001 1....... ........ */
                /* la.decode:2153 */
                return LISA_XVABSD_D;
            case 0x4:
                /* 01110100 01100010 0....... ........ */
                /* la.decode:2154 */
                return LISA_XVABSD_BU;
            case 0x5:
                /* 01110100 01100010 1....... ........ */
                /* la.decode:2155 */
                return LISA_XVABSD_HU;
            case 0x6:
                /* 01110100 01100011 0....... ........ */
                /* la.decode:2156 */
                return LISA_XVABSD_WU;
            case 0x7:
                /* 01110100 01100011 1....... ........ */
                /* la.decode:2157 */
                return LISA_XVABSD_DU;
            }
            return LISA_INVALID;
        case 0x19:
            /* 01110100 011001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01100100 0....... ........ */
                /* la.decode:2158 */
                return LISA_XVAVG_B;
            case 0x1:
                /* 01110100 01100100 1....... ........ */
                /* la.decode:2159 */
                return LISA_XVAVG_H;
            case 0x2:
                /* 01110100 01100101 0....... ........ */
                /* la.decode:2160 */
                return LISA_XVAVG_W;
            case 0x3:
                /* 01110100 01100101 1....... ........ */
                /* la.decode:2161 */
                return LISA_XVAVG_D;
            case 0x4:
                /* 01110100 01100110 0....... ........ */
                /* la.decode:2162 */
                return LISA_XVAVG_BU;
            case 0x5:
                /* 01110100 01100110 1....... ........ */
                /* la.decode:2163 */
                return LISA_XVAVG_HU;
            case 0x6:
                /* 01110100 01100111 0....... ........ */
                /* la.decode:2164 */
                return LISA_XVAVG_WU;
            case 0x7:
                /* 01110100 01100111 1....... ........ */
                /* la.decode:2165 */
                return LISA_XVAVG_DU;
            }
            return LISA_INVALID;
        case 0x1a:
            /* 01110100 011010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01101000 0....... ........ */
                /* la.decode:2166 */
                return LISA_XVAVGR_B;
            case 0x1:
                /* 01110100 01101000 1....... ........ */
                /* la.decode:2167 */
                return LISA_XVAVGR_H;
            case 0x2:
                /* 01110100 01101001 0....... ........ */
                /* la.decode:2168 */
                return LISA_XVAVGR_W;
            case 0x3:
                /* 01110100 01101001 1....... ........ */
                /* la.decode:2169 */
                return LISA_XVAVGR_D;
            case 0x4:
                /* 01110100 01101010 0....... ........ */
                /* la.decode:2170 */
                return LISA_XVAVGR_BU;
            case 0x5:
                /* 01110100 01101010 1....... ........ */
                /* la.decode:2171 */
                return LISA_XVAVGR_HU;
            case 0x6:
                /* 01110100 01101011 0....... ........ */
                /* la.decode:2172 */
                return LISA_XVAVGR_WU;
            case 0x7:
                /* 01110100 01101011 1....... ........ */
                /* la.decode:2173 */
                return LISA_XVAVGR_DU;
            }
            return LISA_INVALID;
        case 0x1b:
            /* 01110100 011011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01101100 0....... ........ */
                /* la.decode:2174 */
                return LISA_XVHALFD_B;
            case 0x1:
                /* 01110100 01101100 1....... ........ */
                /* la.decode:2175 */
                return LISA_XVHALFD_H;
            case 0x2:
                /* 01110100 01101101 0....... ........ */
                /* la.decode:2176 */
                return LISA_XVHALFD_W;
            case 0x3:
                /* 01110100 01101101 1....... ........ */
                /* la.decode:2177 */
                return LISA_XVHALFD_D;
            case 0x4:
                /* 01110100 01101110 0....... ........ */
                /* la.decode:2178 */
                return LISA_XVHALFD_BU;
            case 0x5:
                /* 01110100 01101110 1....... ........ */
                /* la.decode:2179 */
                return LISA_XVHALFD_HU;
            case 0x6:
                /* 01110100 01101111 0....... ........ */
                /* la.decode:2180 */
                return LISA_XVHALFD_WU;
            case 0x7:
                /* 01110100 01101111 1....... ........ */
                /* la.decode:2181 */
                return LISA_XVHALFD_DU;
            }
            return LISA_INVALID;
        case 0x1c:
            /* 01110100 011100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01110000 0....... ........ */
                /* la.decode:2182 */
                return LISA_XVMAX_B;
            case 0x1:
                /* 01110100 01110000 1....... ........ */
                /* la.decode:2183 */
                return LISA_XVMAX_H;
            case 0x2:
                /* 01110100 01110001 0....... ........ */
                /* la.decode:2184 */
                return LISA_XVMAX_W;
            case 0x3:
                /* 01110100 01110001 1....... ........ */
                /* la.decode:2185 */
                return LISA_XVMAX_D;
            case 0x4:
                /* 01110100 01110010 0....... ........ */
                /* la.decode:2186 */
                return LISA_XVMIN_B;
            case 0x5:
                /* 01110100 01110010 1....... ........ */
                /* la.decode:2187 */
                return LISA_XVMIN_H;
            case 0x6:
                /* 01110100 01110011 0....... ........ */
                /* la.decode:2188 */
                return LISA_XVMIN_W;
            case 0x7:
                /* 01110100 01110011 1....... ........ */
                /* la.decode:2189 */
                return LISA_XVMIN_D;
            }
            return LISA_INVALID;
        case 0x1d:
            /* 01110100 011101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01110100 0....... ........ */
                /* la.decode:2190 */
                return LISA_XVMAX_BU;
            case 0x1:
                /* 01110100 01110100 1....... ........ */
                /* la.decode:2191 */
                return LISA_XVMAX_HU;
            case 0x2:
                /* 01110100 01110101 0....... ........ */
                /* la.decode:2192 */
                return LISA_XVMAX_WU;
            case 0x3:
                /* 01110100 01110101 1....... ........ */
                /* la.decode:2193 */
                return LISA_XVMAX_DU;
            case 0x4:
                /* 01110100 01110110 0....... ........ */
                /* la.decode:2194 */
                return LISA_XVMIN_BU;
            case 0x5:
                /* 01110100 01110110 1....... ........ */
                /* la.decode:2195 */
                return LISA_XVMIN_HU;
            case 0x6:
                /* 01110100 01110111 0....... ........ */
                /* la.decode:2196 */
                return LISA_XVMIN_WU;
            case 0x7:
                /* 01110100 01110111 1....... ........ */
                /* la.decode:2197 */
                return LISA_XVMIN_DU;
            }
            return LISA_INVALID;
        case 0x1e:
            /* 01110100 011110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01111000 0....... ........ */
                /* la.decode:2198 */
                return LISA_XVMAXA_B;
            case 0x1:
                /* 01110100 01111000 1....... ........ */
                /* la.decode:2199 */
                return LISA_XVMAXA_H;
            case 0x2:
                /* 01110100 01111001 0....... ........ */
                /* la.decode:2200 */
                return LISA_XVMAXA_W;
            case 0x3:
                /* 01110100 01111001 1....... ........ */
                /* la.decode:2201 */
                return LISA_XVMAXA_D;
            case 0x4:
                /* 01110100 01111010 0....... ........ */
                /* la.decode:2202 */
                return LISA_XVMINA_B;
            case 0x5:
                /* 01110100 01111010 1....... ........ */
                /* la.decode:2203 */
                return LISA_XVMINA_H;
            case 0x6:
                /* 01110100 01111011 0....... ........ */
                /* la.decode:2204 */
                return LISA_XVMINA_W;
            case 0x7:
                /* 01110100 01111011 1....... ........ */
                /* la.decode:2205 */
                return LISA_XVMINA_D;
            }
            return LISA_INVALID;
        case 0x1f:
            /* 01110100 011111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 01111100 0....... ........ */
                /* la.decode:2206 */
                return LISA_XVSADW_H_B;
            case 0x1:
                /* 01110100 01111100 1....... ........ */
                /* la.decode:2207 */
                return LISA_XVSADW_W_H;
            case 0x2:
                /* 01110100 01111101 0....... ........ */
                /* la.decode:2208 */
                return LISA_XVSADW_D_W;
            case 0x4:
                /* 01110100 01111110 0....... ........ */
                /* la.decode:2209 */
                return LISA_XVSADW_H_BU;
            case 0x5:
                /* 01110100 01111110 1....... ........ */
                /* la.decode:2210 */
                return LISA_XVSADW_W_HU;
            case 0x6:
                /* 01110100 01111111 0....... ........ */
                /* la.decode:2211 */
                return LISA_XVSADW_D_WU;
            }
            return LISA_INVALID;
        case 0x20:
            /* 01110100 100000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10000000 0....... ........ */
                /* la.decode:2212 */
                return LISA_XVACCSADW_H_B;
            case 0x1:
                /* 01110100 10000000 1....... ........ */
                /* la.decode:2213 */
                return LISA_XVACCSADW_W_H;
            case 0x2:
                /* 01110100 10000001 0....... ........ */
                /* la.decode:2214 */
                return LISA_XVACCSADW_D_W;
            case 0x4:
                /* 01110100 10000010 0....... ........ */
                /* la.decode:2215 */
                return LISA_XVACCSADW_H_BU;
            case 0x5:
                /* 01110100 10000010 1....... ........ */
                /* la.decode:2216 */
                return LISA_XVACCSADW_W_HU;
            case 0x6:
                /* 01110100 10000011 0....... ........ */
                /* la.decode:2217 */
                return LISA_XVACCSADW_D_WU;
            }
            return LISA_INVALID;
        case 0x21:
            /* 01110100 100001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10000100 0....... ........ */
                /* la.decode:2218 */
                return LISA_XVMUL_B;
            case 0x1:
                /* 01110100 10000100 1....... ........ */
                /* la.decode:2219 */
                return LISA_XVMUL_H;
            case 0x2:
                /* 01110100 10000101 0....... ........ */
                /* la.decode:2220 */
                return LISA_XVMUL_W;
            case 0x3:
                /* 01110100 10000101 1....... ........ */
                /* la.decode:2221 */
                return LISA_XVMUL_D;
            case 0x4:
                /* 01110100 10000110 0....... ........ */
                /* la.decode:2222 */
                return LISA_XVMUH_B;
            case 0x5:
                /* 01110100 10000110 1....... ........ */
                /* la.decode:2223 */
                return LISA_XVMUH_H;
            case 0x6:
                /* 01110100 10000111 0....... ........ */
                /* la.decode:2224 */
                return LISA_XVMUH_W;
            case 0x7:
                /* 01110100 10000111 1....... ........ */
                /* la.decode:2225 */
                return LISA_XVMUH_D;
            }
            return LISA_INVALID;
        case 0x22:
            /* 01110100 100010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10001000 0....... ........ */
                /* la.decode:2226 */
                return LISA_XVMUH_BU;
            case 0x1:
                /* 01110100 10001000 1....... ........ */
                /* la.decode:2227 */
                return LISA_XVMUH_HU;
            case 0x2:
                /* 01110100 10001001 0....... ........ */
                /* la.decode:2228 */
                return LISA_XVMUH_WU;
            case 0x3:
                /* 01110100 10001001 1....... ........ */
                /* la.decode:2229 */
                return LISA_XVMUH_DU;
            case 0x4:
                /* 01110100 10001010 0....... ........ */
                /* la.decode:2230 */
                return LISA_XVMUH_BU_B;
            case 0x5:
                /* 01110100 10001010 1....... ........ */
                /* la.decode:2231 */
                return LISA_XVMUH_HU_H;
            case 0x6:
                /* 01110100 10001011 0....... ........ */
                /* la.decode:2232 */
                return LISA_XVMUH_WU_W;
            case 0x7:
                /* 01110100 10001011 1....... ........ */
                /* la.decode:2233 */
                return LISA_XVMUH_DU_D;
            }
            return LISA_INVALID;
        case 0x23:
            /* 01110100 100011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10001100 0....... ........ */
                /* la.decode:2234 */
                return LISA_XVMULXW_H_B;
            case 0x1:
                /* 01110100 10001100 1....... ........ */
                /* la.decode:2235 */
                return LISA_XVMULXW_W_H;
            case 0x2:
                /* 01110100 10001101 0....... ........ */
                /* la.decode:2236 */
                return LISA_XVMULXW_D_W;
            case 0x4:
                /* 01110100 10001110 0....... ........ */
                /* la.decode:2237 */
                return LISA_XVMULXW_H_BU;
            case 0x5:
                /* 01110100 10001110 1....... ........ */
                /* la.decode:2238 */
                return LISA_XVMULXW_W_HU;
            case 0x6:
                /* 01110100 10001111 0....... ........ */
                /* la.decode:2239 */
                return LISA_XVMULXW_D_WU;
            }
            return LISA_INVALID;
        case 0x24:
            /* 01110100 100100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10010000 0....... ........ */
                /* la.decode:2240 */
                return LISA_XVMULWEV_H_B;
            case 0x1:
                /* 01110100 10010000 1....... ........ */
                /* la.decode:2241 */
                return LISA_XVMULWEV_W_H;
            case 0x2:
                /* 01110100 10010001 0....... ........ */
                /* la.decode:2242 */
                return LISA_XVMULWEV_D_W;
            case 0x3:
                /* 01110100 10010001 1....... ........ */
                /* la.decode:2243 */
                return LISA_XVMULWEV_Q_D;
            case 0x4:
                /* 01110100 10010010 0....... ........ */
                /* la.decode:2244 */
                return LISA_XVMULWOD_H_B;
            case 0x5:
                /* 01110100 10010010 1....... ........ */
                /* la.decode:2245 */
                return LISA_XVMULWOD_W_H;
            case 0x6:
                /* 01110100 10010011 0....... ........ */
                /* la.decode:2246 */
                return LISA_XVMULWOD_D_W;
            case 0x7:
                /* 01110100 10010011 1....... ........ */
                /* la.decode:2247 */
                return LISA_XVMULWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x25:
            /* 01110100 100101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10010100 0....... ........ */
                /* la.decode:2248 */
                return LISA_XVMULWL_H_B;
            case 0x1:
                /* 01110100 10010100 1....... ........ */
                /* la.decode:2249 */
                return LISA_XVMULWL_W_H;
            case 0x2:
                /* 01110100 10010101 0....... ........ */
                /* la.decode:2250 */
                return LISA_XVMULWL_D_W;
            case 0x3:
                /* 01110100 10010101 1....... ........ */
                /* la.decode:2251 */
                return LISA_XVMULWL_Q_D;
            case 0x4:
                /* 01110100 10010110 0....... ........ */
                /* la.decode:2252 */
                return LISA_XVMULWH_H_B;
            case 0x5:
                /* 01110100 10010110 1....... ........ */
                /* la.decode:2253 */
                return LISA_XVMULWH_W_H;
            case 0x6:
                /* 01110100 10010111 0....... ........ */
                /* la.decode:2254 */
                return LISA_XVMULWH_D_W;
            case 0x7:
                /* 01110100 10010111 1....... ........ */
                /* la.decode:2255 */
                return LISA_XVMULWH_Q_D;
            }
            return LISA_INVALID;
        case 0x26:
            /* 01110100 100110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10011000 0....... ........ */
                /* la.decode:2256 */
                return LISA_XVMULWEV_H_BU;
            case 0x1:
                /* 01110100 10011000 1....... ........ */
                /* la.decode:2257 */
                return LISA_XVMULWEV_W_HU;
            case 0x2:
                /* 01110100 10011001 0....... ........ */
                /* la.decode:2258 */
                return LISA_XVMULWEV_D_WU;
            case 0x3:
                /* 01110100 10011001 1....... ........ */
                /* la.decode:2259 */
                return LISA_XVMULWEV_Q_DU;
            case 0x4:
                /* 01110100 10011010 0....... ........ */
                /* la.decode:2260 */
                return LISA_XVMULWOD_H_BU;
            case 0x5:
                /* 01110100 10011010 1....... ........ */
                /* la.decode:2261 */
                return LISA_XVMULWOD_W_HU;
            case 0x6:
                /* 01110100 10011011 0....... ........ */
                /* la.decode:2262 */
                return LISA_XVMULWOD_D_WU;
            case 0x7:
                /* 01110100 10011011 1....... ........ */
                /* la.decode:2263 */
                return LISA_XVMULWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0x27:
            /* 01110100 100111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10011100 0....... ........ */
                /* la.decode:2264 */
                return LISA_XVMULWL_H_BU;
            case 0x1:
                /* 01110100 10011100 1....... ........ */
                /* la.decode:2265 */
                return LISA_XVMULWL_W_HU;
            case 0x2:
                /* 01110100 10011101 0....... ........ */
                /* la.decode:2266 */
                return LISA_XVMULWL_D_WU;
            case 0x3:
                /* 01110100 10011101 1....... ........ */
                /* la.decode:2267 */
                return LISA_XVMULWL_Q_DU;
            case 0x4:
                /* 01110100 10011110 0....... ........ */
                /* la.decode:2268 */
                return LISA_XVMULWH_H_BU;
            case 0x5:
                /* 01110100 10011110 1....... ........ */
                /* la.decode:2269 */
                return LISA_XVMULWH_W_HU;
            case 0x6:
                /* 01110100 10011111 0....... ........ */
                /* la.decode:2270 */
                return LISA_XVMULWH_D_WU;
            case 0x7:
                /* 01110100 10011111 1....... ........ */
                /* la.decode:2271 */
                return LISA_XVMULWH_Q_DU;
            }
            return LISA_INVALID;
        case 0x28:
            /* 01110100 101000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10100000 0....... ........ */
                /* la.decode:2272 */
                return LISA_XVMULWEV_H_BU_B;
            case 0x1:
                /* 01110100 10100000 1....... ........ */
                /* la.decode:2273 */
                return LISA_XVMULWEV_W_HU_H;
            case 0x2:
                /* 01110100 10100001 0....... ........ */
                /* la.decode:2274 */
                return LISA_XVMULWEV_D_WU_W;
            case 0x3:
                /* 01110100 10100001 1....... ........ */
                /* la.decode:2275 */
                return LISA_XVMULWEV_Q_DU_D;
            case 0x4:
                /* 01110100 10100010 0....... ........ */
                /* la.decode:2276 */
                return LISA_XVMULWOD_H_BU_B;
            case 0x5:
                /* 01110100 10100010 1....... ........ */
                /* la.decode:2277 */
                return LISA_XVMULWOD_W_HU_H;
            case 0x6:
                /* 01110100 10100011 0....... ........ */
                /* la.decode:2278 */
                return LISA_XVMULWOD_D_WU_W;
            case 0x7:
                /* 01110100 10100011 1....... ........ */
                /* la.decode:2279 */
                return LISA_XVMULWOD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x29:
            /* 01110100 101001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10100100 0....... ........ */
                /* la.decode:2280 */
                return LISA_XVMULWL_H_BU_B;
            case 0x1:
                /* 01110100 10100100 1....... ........ */
                /* la.decode:2281 */
                return LISA_XVMULWL_W_HU_H;
            case 0x2:
                /* 01110100 10100101 0....... ........ */
                /* la.decode:2282 */
                return LISA_XVMULWL_D_WU_W;
            case 0x3:
                /* 01110100 10100101 1....... ........ */
                /* la.decode:2283 */
                return LISA_XVMULWL_Q_DU_D;
            case 0x4:
                /* 01110100 10100110 0....... ........ */
                /* la.decode:2284 */
                return LISA_XVMULWH_H_BU_B;
            case 0x5:
                /* 01110100 10100110 1....... ........ */
                /* la.decode:2285 */
                return LISA_XVMULWH_W_HU_H;
            case 0x6:
                /* 01110100 10100111 0....... ........ */
                /* la.decode:2286 */
                return LISA_XVMULWH_D_WU_W;
            case 0x7:
                /* 01110100 10100111 1....... ........ */
                /* la.decode:2287 */
                return LISA_XVMULWH_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x2a:
            /* 01110100 101010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10101000 0....... ........ */
                /* la.decode:2288 */
                return LISA_XVMADD_B;
            case 0x1:
                /* 01110100 10101000 1....... ........ */
                /* la.decode:2289 */
                return LISA_XVMADD_H;
            case 0x2:
                /* 01110100 10101001 0....... ........ */
                /* la.decode:2290 */
                return LISA_XVMADD_W;
            case 0x3:
                /* 01110100 10101001 1....... ........ */
                /* la.decode:2291 */
                return LISA_XVMADD_D;
            case 0x4:
                /* 01110100 10101010 0....... ........ */
                /* la.decode:2292 */
                return LISA_XVMSUB_B;
            case 0x5:
                /* 01110100 10101010 1....... ........ */
                /* la.decode:2293 */
                return LISA_XVMSUB_H;
            case 0x6:
                /* 01110100 10101011 0....... ........ */
                /* la.decode:2294 */
                return LISA_XVMSUB_W;
            case 0x7:
                /* 01110100 10101011 1....... ........ */
                /* la.decode:2295 */
                return LISA_XVMSUB_D;
            }
            return LISA_INVALID;
        case 0x2b:
            /* 01110100 101011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10101100 0....... ........ */
                /* la.decode:2296 */
                return LISA_XVMADDWEV_H_B;
            case 0x1:
                /* 01110100 10101100 1....... ........ */
                /* la.decode:2297 */
                return LISA_XVMADDWEV_W_H;
            case 0x2:
                /* 01110100 10101101 0....... ........ */
                /* la.decode:2298 */
                return LISA_XVMADDWEV_D_W;
            case 0x3:
                /* 01110100 10101101 1....... ........ */
                /* la.decode:2299 */
                return LISA_XVMADDWEV_Q_D;
            case 0x4:
                /* 01110100 10101110 0....... ........ */
                /* la.decode:2300 */
                return LISA_XVMADDWOD_H_B;
            case 0x5:
                /* 01110100 10101110 1....... ........ */
                /* la.decode:2301 */
                return LISA_XVMADDWOD_W_H;
            case 0x6:
                /* 01110100 10101111 0....... ........ */
                /* la.decode:2302 */
                return LISA_XVMADDWOD_D_W;
            case 0x7:
                /* 01110100 10101111 1....... ........ */
                /* la.decode:2303 */
                return LISA_XVMADDWOD_Q_D;
            }
            return LISA_INVALID;
        case 0x2c:
            /* 01110100 101100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10110000 0....... ........ */
                /* la.decode:2304 */
                return LISA_XVMADDWL_H_B;
            case 0x1:
                /* 01110100 10110000 1....... ........ */
                /* la.decode:2305 */
                return LISA_XVMADDWL_W_H;
            case 0x2:
                /* 01110100 10110001 0....... ........ */
                /* la.decode:2306 */
                return LISA_XVMADDWL_D_W;
            case 0x3:
                /* 01110100 10110001 1....... ........ */
                /* la.decode:2307 */
                return LISA_XVMADDWL_Q_D;
            case 0x4:
                /* 01110100 10110010 0....... ........ */
                /* la.decode:2308 */
                return LISA_XVMADDWH_H_B;
            case 0x5:
                /* 01110100 10110010 1....... ........ */
                /* la.decode:2309 */
                return LISA_XVMADDWH_W_H;
            case 0x6:
                /* 01110100 10110011 0....... ........ */
                /* la.decode:2310 */
                return LISA_XVMADDWH_D_W;
            case 0x7:
                /* 01110100 10110011 1....... ........ */
                /* la.decode:2311 */
                return LISA_XVMADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0x2d:
            /* 01110100 101101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10110100 0....... ........ */
                /* la.decode:2312 */
                return LISA_XVMADDWEV_H_BU;
            case 0x1:
                /* 01110100 10110100 1....... ........ */
                /* la.decode:2313 */
                return LISA_XVMADDWEV_W_HU;
            case 0x2:
                /* 01110100 10110101 0....... ........ */
                /* la.decode:2314 */
                return LISA_XVMADDWEV_D_WU;
            case 0x3:
                /* 01110100 10110101 1....... ........ */
                /* la.decode:2315 */
                return LISA_XVMADDWEV_Q_DU;
            case 0x4:
                /* 01110100 10110110 0....... ........ */
                /* la.decode:2316 */
                return LISA_XVMADDWOD_H_BU;
            case 0x5:
                /* 01110100 10110110 1....... ........ */
                /* la.decode:2317 */
                return LISA_XVMADDWOD_W_HU;
            case 0x6:
                /* 01110100 10110111 0....... ........ */
                /* la.decode:2318 */
                return LISA_XVMADDWOD_D_WU;
            case 0x7:
                /* 01110100 10110111 1....... ........ */
                /* la.decode:2319 */
                return LISA_XVMADDWOD_Q_DU;
            }
            return LISA_INVALID;
        case 0x2e:
            /* 01110100 101110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10111000 0....... ........ */
                /* la.decode:2320 */
                return LISA_XVMADDWL_H_BU;
            case 0x1:
                /* 01110100 10111000 1....... ........ */
                /* la.decode:2321 */
                return LISA_XVMADDWL_W_HU;
            case 0x2:
                /* 01110100 10111001 0....... ........ */
                /* la.decode:2322 */
                return LISA_XVMADDWL_D_WU;
            case 0x3:
                /* 01110100 10111001 1....... ........ */
                /* la.decode:2323 */
                return LISA_XVMADDWL_Q_DU;
            case 0x4:
                /* 01110100 10111010 0....... ........ */
                /* la.decode:2324 */
                return LISA_XVMADDWH_H_BU;
            case 0x5:
                /* 01110100 10111010 1....... ........ */
                /* la.decode:2325 */
                return LISA_XVMADDWH_W_HU;
            case 0x6:
                /* 01110100 10111011 0....... ........ */
                /* la.decode:2326 */
                return LISA_XVMADDWH_D_WU;
            case 0x7:
                /* 01110100 10111011 1....... ........ */
                /* la.decode:2327 */
                return LISA_XVMADDWH_Q_DU;
            }
            return LISA_INVALID;
        case 0x2f:
            /* 01110100 101111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 10111100 0....... ........ */
                /* la.decode:2328 */
                return LISA_XVMADDWEV_H_BU_B;
            case 0x1:
                /* 01110100 10111100 1....... ........ */
                /* la.decode:2329 */
                return LISA_XVMADDWEV_W_HU_H;
            case 0x2:
                /* 01110100 10111101 0....... ........ */
                /* la.decode:2330 */
                return LISA_XVMADDWEV_D_WU_W;
            case 0x3:
                /* 01110100 10111101 1....... ........ */
                /* la.decode:2331 */
                return LISA_XVMADDWEV_Q_DU_D;
            case 0x4:
                /* 01110100 10111110 0....... ........ */
                /* la.decode:2332 */
                return LISA_XVMADDWOD_H_BU_B;
            case 0x5:
                /* 01110100 10111110 1....... ........ */
                /* la.decode:2333 */
                return LISA_XVMADDWOD_W_HU_H;
            case 0x6:
                /* 01110100 10111111 0....... ........ */
                /* la.decode:2334 */
                return LISA_XVMADDWOD_D_WU_W;
            case 0x7:
                /* 01110100 10111111 1....... ........ */
                /* la.decode:2335 */
                return LISA_XVMADDWOD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x30:
            /* 01110100 110000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11000000 0....... ........ */
                /* la.decode:2336 */
                return LISA_XVMADDWL_H_BU_B;
            case 0x1:
                /* 01110100 11000000 1....... ........ */
                /* la.decode:2337 */
                return LISA_XVMADDWL_W_HU_H;
            case 0x2:
                /* 01110100 11000001 0....... ........ */
                /* la.decode:2338 */
                return LISA_XVMADDWL_D_WU_W;
            case 0x3:
                /* 01110100 11000001 1....... ........ */
                /* la.decode:2339 */
                return LISA_XVMADDWL_Q_DU_D;
            case 0x4:
                /* 01110100 11000010 0....... ........ */
                /* la.decode:2340 */
                return LISA_XVMADDWH_H_BU_B;
            case 0x5:
                /* 01110100 11000010 1....... ........ */
                /* la.decode:2341 */
                return LISA_XVMADDWH_W_HU_H;
            case 0x6:
                /* 01110100 11000011 0....... ........ */
                /* la.decode:2342 */
                return LISA_XVMADDWH_D_WU_W;
            case 0x7:
                /* 01110100 11000011 1....... ........ */
                /* la.decode:2343 */
                return LISA_XVMADDWH_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x31:
            /* 01110100 110001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11000100 0....... ........ */
                /* la.decode:2344 */
                return LISA_XVDP2_H_B;
            case 0x1:
                /* 01110100 11000100 1....... ........ */
                /* la.decode:2345 */
                return LISA_XVDP2_W_H;
            case 0x2:
                /* 01110100 11000101 0....... ........ */
                /* la.decode:2346 */
                return LISA_XVDP2_D_W;
            case 0x3:
                /* 01110100 11000101 1....... ........ */
                /* la.decode:2347 */
                return LISA_XVDP2_Q_D;
            case 0x4:
                /* 01110100 11000110 0....... ........ */
                /* la.decode:2348 */
                return LISA_XVDP2_HU_BU;
            case 0x5:
                /* 01110100 11000110 1....... ........ */
                /* la.decode:2349 */
                return LISA_XVDP2_WU_HU;
            case 0x6:
                /* 01110100 11000111 0....... ........ */
                /* la.decode:2350 */
                return LISA_XVDP2_DU_WU;
            case 0x7:
                /* 01110100 11000111 1....... ........ */
                /* la.decode:2351 */
                return LISA_XVDP2_QU_DU;
            }
            return LISA_INVALID;
        case 0x32:
            /* 01110100 110010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11001000 0....... ........ */
                /* la.decode:2352 */
                return LISA_XVDP2_H_BU_B;
            case 0x1:
                /* 01110100 11001000 1....... ........ */
                /* la.decode:2353 */
                return LISA_XVDP2_W_HU_H;
            case 0x2:
                /* 01110100 11001001 0....... ........ */
                /* la.decode:2354 */
                return LISA_XVDP2_D_WU_W;
            case 0x3:
                /* 01110100 11001001 1....... ........ */
                /* la.decode:2355 */
                return LISA_XVDP2_Q_DU_D;
            case 0x4:
                /* 01110100 11001010 0....... ........ */
                /* la.decode:2356 */
                return LISA_XVDP2ADD_H_B;
            case 0x5:
                /* 01110100 11001010 1....... ........ */
                /* la.decode:2357 */
                return LISA_XVDP2ADD_W_H;
            case 0x6:
                /* 01110100 11001011 0....... ........ */
                /* la.decode:2358 */
                return LISA_XVDP2ADD_D_W;
            case 0x7:
                /* 01110100 11001011 1....... ........ */
                /* la.decode:2359 */
                return LISA_XVDP2ADD_Q_D;
            }
            return LISA_INVALID;
        case 0x33:
            /* 01110100 110011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11001100 0....... ........ */
                /* la.decode:2360 */
                return LISA_XVDP2ADD_H_BU;
            case 0x1:
                /* 01110100 11001100 1....... ........ */
                /* la.decode:2361 */
                return LISA_XVDP2ADD_W_HU;
            case 0x2:
                /* 01110100 11001101 0....... ........ */
                /* la.decode:2362 */
                return LISA_XVDP2ADD_D_WU;
            case 0x3:
                /* 01110100 11001101 1....... ........ */
                /* la.decode:2363 */
                return LISA_XVDP2ADD_Q_DU;
            case 0x4:
                /* 01110100 11001110 0....... ........ */
                /* la.decode:2364 */
                return LISA_XVDP2ADD_H_BU_B;
            case 0x5:
                /* 01110100 11001110 1....... ........ */
                /* la.decode:2365 */
                return LISA_XVDP2ADD_W_HU_H;
            case 0x6:
                /* 01110100 11001111 0....... ........ */
                /* la.decode:2366 */
                return LISA_XVDP2ADD_D_WU_W;
            case 0x7:
                /* 01110100 11001111 1....... ........ */
                /* la.decode:2367 */
                return LISA_XVDP2ADD_Q_DU_D;
            }
            return LISA_INVALID;
        case 0x34:
            /* 01110100 110100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11010000 0....... ........ */
                /* la.decode:2368 */
                return LISA_XVDP2SUB_H_B;
            case 0x1:
                /* 01110100 11010000 1....... ........ */
                /* la.decode:2369 */
                return LISA_XVDP2SUB_W_H;
            case 0x2:
                /* 01110100 11010001 0....... ........ */
                /* la.decode:2370 */
                return LISA_XVDP2SUB_D_W;
            case 0x3:
                /* 01110100 11010001 1....... ........ */
                /* la.decode:2371 */
                return LISA_XVDP2SUB_Q_D;
            case 0x4:
                /* 01110100 11010010 0....... ........ */
                /* la.decode:2372 */
                return LISA_XVDP2SUB_H_BU;
            case 0x5:
                /* 01110100 11010010 1....... ........ */
                /* la.decode:2373 */
                return LISA_XVDP2SUB_W_HU;
            case 0x6:
                /* 01110100 11010011 0....... ........ */
                /* la.decode:2374 */
                return LISA_XVDP2SUB_D_WU;
            case 0x7:
                /* 01110100 11010011 1....... ........ */
                /* la.decode:2375 */
                return LISA_XVDP2SUB_Q_DU;
            }
            return LISA_INVALID;
        case 0x35:
            /* 01110100 110101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11010100 0....... ........ */
                /* la.decode:2376 */
                return LISA_XVDP4_W_B;
            case 0x1:
                /* 01110100 11010100 1....... ........ */
                /* la.decode:2377 */
                return LISA_XVDP4_D_H;
            case 0x2:
                /* 01110100 11010101 0....... ........ */
                /* la.decode:2378 */
                return LISA_XVDP4_Q_W;
            case 0x4:
                /* 01110100 11010110 0....... ........ */
                /* la.decode:2379 */
                return LISA_XVDP4_W_BU;
            case 0x5:
                /* 01110100 11010110 1....... ........ */
                /* la.decode:2380 */
                return LISA_XVDP4_D_HU;
            case 0x6:
                /* 01110100 11010111 0....... ........ */
                /* la.decode:2381 */
                return LISA_XVDP4_Q_WU;
            }
            return LISA_INVALID;
        case 0x36:
            /* 01110100 110110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11011000 0....... ........ */
                /* la.decode:2382 */
                return LISA_XVDP4_W_BU_B;
            case 0x1:
                /* 01110100 11011000 1....... ........ */
                /* la.decode:2383 */
                return LISA_XVDP4_D_HU_H;
            case 0x2:
                /* 01110100 11011001 0....... ........ */
                /* la.decode:2384 */
                return LISA_XVDP4_Q_WU_W;
            case 0x4:
                /* 01110100 11011010 0....... ........ */
                /* la.decode:2385 */
                return LISA_XVDP4ADD_W_B;
            case 0x5:
                /* 01110100 11011010 1....... ........ */
                /* la.decode:2386 */
                return LISA_XVDP4ADD_D_H;
            case 0x6:
                /* 01110100 11011011 0....... ........ */
                /* la.decode:2387 */
                return LISA_XVDP4ADD_Q_W;
            }
            return LISA_INVALID;
        case 0x37:
            /* 01110100 110111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11011100 0....... ........ */
                /* la.decode:2388 */
                return LISA_XVDP4ADD_W_BU;
            case 0x1:
                /* 01110100 11011100 1....... ........ */
                /* la.decode:2389 */
                return LISA_XVDP4ADD_D_HU;
            case 0x2:
                /* 01110100 11011101 0....... ........ */
                /* la.decode:2390 */
                return LISA_XVDP4ADD_Q_WU;
            case 0x4:
                /* 01110100 11011110 0....... ........ */
                /* la.decode:2391 */
                return LISA_XVDP4ADD_W_BU_B;
            case 0x5:
                /* 01110100 11011110 1....... ........ */
                /* la.decode:2392 */
                return LISA_XVDP4ADD_D_HU_H;
            case 0x6:
                /* 01110100 11011111 0....... ........ */
                /* la.decode:2393 */
                return LISA_XVDP4ADD_Q_WU_W;
            }
            return LISA_INVALID;
        case 0x38:
            /* 01110100 111000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11100000 0....... ........ */
                /* la.decode:2394 */
                return LISA_XVDIV_B;
            case 0x1:
                /* 01110100 11100000 1....... ........ */
                /* la.decode:2395 */
                return LISA_XVDIV_H;
            case 0x2:
                /* 01110100 11100001 0....... ........ */
                /* la.decode:2396 */
                return LISA_XVDIV_W;
            case 0x3:
                /* 01110100 11100001 1....... ........ */
                /* la.decode:2397 */
                return LISA_XVDIV_D;
            case 0x4:
                /* 01110100 11100010 0....... ........ */
                /* la.decode:2398 */
                return LISA_XVMOD_B;
            case 0x5:
                /* 01110100 11100010 1....... ........ */
                /* la.decode:2399 */
                return LISA_XVMOD_H;
            case 0x6:
                /* 01110100 11100011 0....... ........ */
                /* la.decode:2400 */
                return LISA_XVMOD_W;
            case 0x7:
                /* 01110100 11100011 1....... ........ */
                /* la.decode:2401 */
                return LISA_XVMOD_D;
            }
            return LISA_INVALID;
        case 0x39:
            /* 01110100 111001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11100100 0....... ........ */
                /* la.decode:2402 */
                return LISA_XVDIV_BU;
            case 0x1:
                /* 01110100 11100100 1....... ........ */
                /* la.decode:2403 */
                return LISA_XVDIV_HU;
            case 0x2:
                /* 01110100 11100101 0....... ........ */
                /* la.decode:2404 */
                return LISA_XVDIV_WU;
            case 0x3:
                /* 01110100 11100101 1....... ........ */
                /* la.decode:2405 */
                return LISA_XVDIV_DU;
            case 0x4:
                /* 01110100 11100110 0....... ........ */
                /* la.decode:2406 */
                return LISA_XVMOD_BU;
            case 0x5:
                /* 01110100 11100110 1....... ........ */
                /* la.decode:2407 */
                return LISA_XVMOD_HU;
            case 0x6:
                /* 01110100 11100111 0....... ........ */
                /* la.decode:2408 */
                return LISA_XVMOD_WU;
            case 0x7:
                /* 01110100 11100111 1....... ........ */
                /* la.decode:2409 */
                return LISA_XVMOD_DU;
            }
            return LISA_INVALID;
        case 0x3a:
            /* 01110100 111010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11101000 0....... ........ */
                /* la.decode:2410 */
                return LISA_XVSLL_B;
            case 0x1:
                /* 01110100 11101000 1....... ........ */
                /* la.decode:2411 */
                return LISA_XVSLL_H;
            case 0x2:
                /* 01110100 11101001 0....... ........ */
                /* la.decode:2412 */
                return LISA_XVSLL_W;
            case 0x3:
                /* 01110100 11101001 1....... ........ */
                /* la.decode:2413 */
                return LISA_XVSLL_D;
            case 0x4:
                /* 01110100 11101010 0....... ........ */
                /* la.decode:2414 */
                return LISA_XVSRL_B;
            case 0x5:
                /* 01110100 11101010 1....... ........ */
                /* la.decode:2415 */
                return LISA_XVSRL_H;
            case 0x6:
                /* 01110100 11101011 0....... ........ */
                /* la.decode:2416 */
                return LISA_XVSRL_W;
            case 0x7:
                /* 01110100 11101011 1....... ........ */
                /* la.decode:2417 */
                return LISA_XVSRL_D;
            }
            return LISA_INVALID;
        case 0x3b:
            /* 01110100 111011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11101100 0....... ........ */
                /* la.decode:2418 */
                return LISA_XVSRA_B;
            case 0x1:
                /* 01110100 11101100 1....... ........ */
                /* la.decode:2419 */
                return LISA_XVSRA_H;
            case 0x2:
                /* 01110100 11101101 0....... ........ */
                /* la.decode:2420 */
                return LISA_XVSRA_W;
            case 0x3:
                /* 01110100 11101101 1....... ........ */
                /* la.decode:2421 */
                return LISA_XVSRA_D;
            case 0x4:
                /* 01110100 11101110 0....... ........ */
                /* la.decode:2422 */
                return LISA_XVROTR_B;
            case 0x5:
                /* 01110100 11101110 1....... ........ */
                /* la.decode:2423 */
                return LISA_XVROTR_H;
            case 0x6:
                /* 01110100 11101111 0....... ........ */
                /* la.decode:2424 */
                return LISA_XVROTR_W;
            case 0x7:
                /* 01110100 11101111 1....... ........ */
                /* la.decode:2425 */
                return LISA_XVROTR_D;
            }
            return LISA_INVALID;
        case 0x3c:
            /* 01110100 111100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110100 11110000 0....... ........ */
                /* la.decode:2426 */
                return LISA_XVSRLR_B;
            case 0x1:
                /* 01110100 11110000 1....... ........ */
                /* la.decode:2427 */
                return LISA_XVSRLR_H;
            case 0x2:
                /* 01110100 11110001 0....... ........ */
                /* la.decode:2428 */
                return LISA_XVSRLR_W;
            case 0x3:
                /* 01110100 11110001 1....... ........ */
                /* la.decode:2429 */
                return LISA_XVSRLR_D;
            case 0x4:
                /* 01110100 11110010 0....... ........ */
                /* la.decode:2430 */
                return LISA_XVSRAR_B;
            case 0x5:
                /* 01110100 11110010 1....... ........ */
                /* la.decode:2431 */
                return LISA_XVSRAR_H;
            case 0x6:
                /* 01110100 11110011 0....... ........ */
                /* la.decode:2432 */
                return LISA_XVSRAR_W;
            case 0x7:
                /* 01110100 11110011 1....... ........ */
                /* la.decode:2433 */
                return LISA_XVSRAR_D;
            }
            return LISA_INVALID;
        case 0x3d:
            /* 01110100 111101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110100 11110100 1....... ........ */
                /* la.decode:2434 */
                return LISA_XVSRLN_B_H;
            case 0x2:
                /* 01110100 11110101 0....... ........ */
                /* la.decode:2435 */
                return LISA_XVSRLN_H_W;
            case 0x3:
                /* 01110100 11110101 1....... ........ */
                /* la.decode:2436 */
                return LISA_XVSRLN_W_D;
            case 0x5:
                /* 01110100 11110110 1....... ........ */
                /* la.decode:2437 */
                return LISA_XVSRAN_B_H;
            case 0x6:
                /* 01110100 11110111 0....... ........ */
                /* la.decode:2438 */
                return LISA_XVSRAN_H_W;
            case 0x7:
                /* 01110100 11110111 1....... ........ */
                /* la.decode:2439 */
                return LISA_XVSRAN_W_D;
            }
            return LISA_INVALID;
        case 0x3e:
            /* 01110100 111110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110100 11111000 1....... ........ */
                /* la.decode:2440 */
                return LISA_XVSRLRN_B_H;
            case 0x2:
                /* 01110100 11111001 0....... ........ */
                /* la.decode:2441 */
                return LISA_XVSRLRN_H_W;
            case 0x3:
                /* 01110100 11111001 1....... ........ */
                /* la.decode:2442 */
                return LISA_XVSRLRN_W_D;
            case 0x5:
                /* 01110100 11111010 1....... ........ */
                /* la.decode:2443 */
                return LISA_XVSRARN_B_H;
            case 0x6:
                /* 01110100 11111011 0....... ........ */
                /* la.decode:2444 */
                return LISA_XVSRARN_H_W;
            case 0x7:
                /* 01110100 11111011 1....... ........ */
                /* la.decode:2445 */
                return LISA_XVSRARN_W_D;
            }
            return LISA_INVALID;
        case 0x3f:
            /* 01110100 111111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110100 11111100 1....... ........ */
                /* la.decode:2446 */
                return LISA_XVSSRLN_B_H;
            case 0x2:
                /* 01110100 11111101 0....... ........ */
                /* la.decode:2447 */
                return LISA_XVSSRLN_H_W;
            case 0x3:
                /* 01110100 11111101 1....... ........ */
                /* la.decode:2448 */
                return LISA_XVSSRLN_W_D;
            case 0x5:
                /* 01110100 11111110 1....... ........ */
                /* la.decode:2449 */
                return LISA_XVSSRAN_B_H;
            case 0x6:
                /* 01110100 11111111 0....... ........ */
                /* la.decode:2450 */
                return LISA_XVSSRAN_H_W;
            case 0x7:
                /* 01110100 11111111 1....... ........ */
                /* la.decode:2451 */
                return LISA_XVSSRAN_W_D;
            }
            return LISA_INVALID;
        case 0x40:
            /* 01110101 000000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110101 00000000 1....... ........ */
                /* la.decode:2452 */
                return LISA_XVSSRLRN_B_H;
            case 0x2:
                /* 01110101 00000001 0....... ........ */
                /* la.decode:2453 */
                return LISA_XVSSRLRN_H_W;
            case 0x3:
                /* 01110101 00000001 1....... ........ */
                /* la.decode:2454 */
                return LISA_XVSSRLRN_W_D;
            case 0x5:
                /* 01110101 00000010 1....... ........ */
                /* la.decode:2455 */
                return LISA_XVSSRARN_B_H;
            case 0x6:
                /* 01110101 00000011 0....... ........ */
                /* la.decode:2456 */
                return LISA_XVSSRARN_H_W;
            case 0x7:
                /* 01110101 00000011 1....... ........ */
                /* la.decode:2457 */
                return LISA_XVSSRARN_W_D;
            }
            return LISA_INVALID;
        case 0x41:
            /* 01110101 000001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110101 00000100 1....... ........ */
                /* la.decode:2458 */
                return LISA_XVSSRLN_BU_H;
            case 0x2:
                /* 01110101 00000101 0....... ........ */
                /* la.decode:2459 */
                return LISA_XVSSRLN_HU_W;
            case 0x3:
                /* 01110101 00000101 1....... ........ */
                /* la.decode:2460 */
                return LISA_XVSSRLN_WU_D;
            case 0x5:
                /* 01110101 00000110 1....... ........ */
                /* la.decode:2461 */
                return LISA_XVSSRAN_BU_H;
            case 0x6:
                /* 01110101 00000111 0....... ........ */
                /* la.decode:2462 */
                return LISA_XVSSRAN_HU_W;
            case 0x7:
                /* 01110101 00000111 1....... ........ */
                /* la.decode:2463 */
                return LISA_XVSSRAN_WU_D;
            }
            return LISA_INVALID;
        case 0x42:
            /* 01110101 000010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x1:
                /* 01110101 00001000 1....... ........ */
                /* la.decode:2464 */
                return LISA_XVSSRLRN_BU_H;
            case 0x2:
                /* 01110101 00001001 0....... ........ */
                /* la.decode:2465 */
                return LISA_XVSSRLRN_HU_W;
            case 0x3:
                /* 01110101 00001001 1....... ........ */
                /* la.decode:2466 */
                return LISA_XVSSRLRN_WU_D;
            case 0x5:
                /* 01110101 00001010 1....... ........ */
                /* la.decode:2467 */
                return LISA_XVSSRARN_BU_H;
            case 0x6:
                /* 01110101 00001011 0....... ........ */
                /* la.decode:2468 */
                return LISA_XVSSRARN_HU_W;
            case 0x7:
                /* 01110101 00001011 1....... ........ */
                /* la.decode:2469 */
                return LISA_XVSSRARN_WU_D;
            }
            return LISA_INVALID;
        case 0x43:
            /* 01110101 000011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00001100 0....... ........ */
                /* la.decode:2470 */
                return LISA_XVBITCLR_B;
            case 0x1:
                /* 01110101 00001100 1....... ........ */
                /* la.decode:2471 */
                return LISA_XVBITCLR_H;
            case 0x2:
                /* 01110101 00001101 0....... ........ */
                /* la.decode:2472 */
                return LISA_XVBITCLR_W;
            case 0x3:
                /* 01110101 00001101 1....... ........ */
                /* la.decode:2473 */
                return LISA_XVBITCLR_D;
            case 0x4:
                /* 01110101 00001110 0....... ........ */
                /* la.decode:2474 */
                return LISA_XVBITSET_B;
            case 0x5:
                /* 01110101 00001110 1....... ........ */
                /* la.decode:2475 */
                return LISA_XVBITSET_H;
            case 0x6:
                /* 01110101 00001111 0....... ........ */
                /* la.decode:2476 */
                return LISA_XVBITSET_W;
            case 0x7:
                /* 01110101 00001111 1....... ........ */
                /* la.decode:2477 */
                return LISA_XVBITSET_D;
            }
            return LISA_INVALID;
        case 0x44:
            /* 01110101 000100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00010000 0....... ........ */
                /* la.decode:2478 */
                return LISA_XVBITREV_B;
            case 0x1:
                /* 01110101 00010000 1....... ........ */
                /* la.decode:2479 */
                return LISA_XVBITREV_H;
            case 0x2:
                /* 01110101 00010001 0....... ........ */
                /* la.decode:2480 */
                return LISA_XVBITREV_W;
            case 0x3:
                /* 01110101 00010001 1....... ........ */
                /* la.decode:2481 */
                return LISA_XVBITREV_D;
            case 0x4:
                /* 01110101 00010010 0....... ........ */
                /* la.decode:2482 */
                return LISA_XVBSTRC12_B;
            case 0x5:
                /* 01110101 00010010 1....... ........ */
                /* la.decode:2483 */
                return LISA_XVBSTRC12_H;
            case 0x6:
                /* 01110101 00010011 0....... ........ */
                /* la.decode:2484 */
                return LISA_XVBSTRC12_W;
            case 0x7:
                /* 01110101 00010011 1....... ........ */
                /* la.decode:2485 */
                return LISA_XVBSTRC12_D;
            }
            return LISA_INVALID;
        case 0x45:
            /* 01110101 000101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00010100 0....... ........ */
                /* la.decode:2486 */
                return LISA_XVBSTRC21_B;
            case 0x1:
                /* 01110101 00010100 1....... ........ */
                /* la.decode:2487 */
                return LISA_XVBSTRC21_H;
            case 0x2:
                /* 01110101 00010101 0....... ........ */
                /* la.decode:2488 */
                return LISA_XVBSTRC21_W;
            case 0x3:
                /* 01110101 00010101 1....... ........ */
                /* la.decode:2489 */
                return LISA_XVBSTRC21_D;
            case 0x4:
                /* 01110101 00010110 0....... ........ */
                /* la.decode:2490 */
                return LISA_XVPACKEV_B;
            case 0x5:
                /* 01110101 00010110 1....... ........ */
                /* la.decode:2491 */
                return LISA_XVPACKEV_H;
            case 0x6:
                /* 01110101 00010111 0....... ........ */
                /* la.decode:2492 */
                return LISA_XVPACKEV_W;
            case 0x7:
                /* 01110101 00010111 1....... ........ */
                /* la.decode:2493 */
                return LISA_XVPACKEV_D;
            }
            return LISA_INVALID;
        case 0x46:
            /* 01110101 000110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00011000 0....... ........ */
                /* la.decode:2494 */
                return LISA_XVPACKOD_B;
            case 0x1:
                /* 01110101 00011000 1....... ........ */
                /* la.decode:2495 */
                return LISA_XVPACKOD_H;
            case 0x2:
                /* 01110101 00011001 0....... ........ */
                /* la.decode:2496 */
                return LISA_XVPACKOD_W;
            case 0x3:
                /* 01110101 00011001 1....... ........ */
                /* la.decode:2497 */
                return LISA_XVPACKOD_D;
            case 0x4:
                /* 01110101 00011010 0....... ........ */
                /* la.decode:2498 */
                return LISA_XVILVL_B;
            case 0x5:
                /* 01110101 00011010 1....... ........ */
                /* la.decode:2499 */
                return LISA_XVILVL_H;
            case 0x6:
                /* 01110101 00011011 0....... ........ */
                /* la.decode:2500 */
                return LISA_XVILVL_W;
            case 0x7:
                /* 01110101 00011011 1....... ........ */
                /* la.decode:2501 */
                return LISA_XVILVL_D;
            }
            return LISA_INVALID;
        case 0x47:
            /* 01110101 000111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00011100 0....... ........ */
                /* la.decode:2502 */
                return LISA_XVILVH_B;
            case 0x1:
                /* 01110101 00011100 1....... ........ */
                /* la.decode:2503 */
                return LISA_XVILVH_H;
            case 0x2:
                /* 01110101 00011101 0....... ........ */
                /* la.decode:2504 */
                return LISA_XVILVH_W;
            case 0x3:
                /* 01110101 00011101 1....... ........ */
                /* la.decode:2505 */
                return LISA_XVILVH_D;
            case 0x4:
                /* 01110101 00011110 0....... ........ */
                /* la.decode:2506 */
                return LISA_XVPICKEV_B;
            case 0x5:
                /* 01110101 00011110 1....... ........ */
                /* la.decode:2507 */
                return LISA_XVPICKEV_H;
            case 0x6:
                /* 01110101 00011111 0....... ........ */
                /* la.decode:2508 */
                return LISA_XVPICKEV_W;
            case 0x7:
                /* 01110101 00011111 1....... ........ */
                /* la.decode:2509 */
                return LISA_XVPICKEV_D;
            }
            return LISA_INVALID;
        case 0x48:
            /* 01110101 001000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00100000 0....... ........ */
                /* la.decode:2510 */
                return LISA_XVPICKOD_B;
            case 0x1:
                /* 01110101 00100000 1....... ........ */
                /* la.decode:2511 */
                return LISA_XVPICKOD_H;
            case 0x2:
                /* 01110101 00100001 0....... ........ */
                /* la.decode:2512 */
                return LISA_XVPICKOD_W;
            case 0x3:
                /* 01110101 00100001 1....... ........ */
                /* la.decode:2513 */
                return LISA_XVPICKOD_D;
            case 0x4:
                /* 01110101 00100010 0....... ........ */
                /* la.decode:2514 */
                return LISA_XVREPLVE_B;
            case 0x5:
                /* 01110101 00100010 1....... ........ */
                /* la.decode:2515 */
                return LISA_XVREPLVE_H;
            case 0x6:
                /* 01110101 00100011 0....... ........ */
                /* la.decode:2516 */
                return LISA_XVREPLVE_W;
            case 0x7:
                /* 01110101 00100011 1....... ........ */
                /* la.decode:2517 */
                return LISA_XVREPLVE_D;
            }
            return LISA_INVALID;
        case 0x49:
            /* 01110101 001001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00100100 0....... ........ */
                /* la.decode:2518 */
                return LISA_XVEXTRCOL_B;
            case 0x1:
                /* 01110101 00100100 1....... ........ */
                /* la.decode:2519 */
                return LISA_XVEXTRCOL_H;
            case 0x2:
                /* 01110101 00100101 0....... ........ */
                /* la.decode:2520 */
                return LISA_XVEXTRCOL_W;
            case 0x3:
                /* 01110101 00100101 1....... ........ */
                /* la.decode:2521 */
                return LISA_XVEXTRCOL_D;
            case 0x4:
                /* 01110101 00100110 0....... ........ */
                /* la.decode:2522 */
                return LISA_XVAND_V;
            case 0x5:
                /* 01110101 00100110 1....... ........ */
                /* la.decode:2523 */
                return LISA_XVOR_V;
            case 0x6:
                /* 01110101 00100111 0....... ........ */
                /* la.decode:2524 */
                return LISA_XVXOR_V;
            case 0x7:
                /* 01110101 00100111 1....... ........ */
                /* la.decode:2525 */
                return LISA_XVNOR_V;
            }
            return LISA_INVALID;
        case 0x4a:
            /* 01110101 001010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00101000 0....... ........ */
                /* la.decode:2526 */
                return LISA_XVANDN_V;
            case 0x1:
                /* 01110101 00101000 1....... ........ */
                /* la.decode:2527 */
                return LISA_XVORN_V;
            case 0x2:
                /* 01110101 00101001 0....... ........ */
                /* la.decode:2528 */
                return LISA_XVRANDSIGN_B;
            case 0x3:
                /* 01110101 00101001 1....... ........ */
                /* la.decode:2529 */
                return LISA_XVRANDSIGN_H;
            case 0x4:
                /* 01110101 00101010 0....... ........ */
                /* la.decode:2530 */
                return LISA_XVRORSIGN_B;
            case 0x5:
                /* 01110101 00101010 1....... ........ */
                /* la.decode:2531 */
                return LISA_XVRORSIGN_H;
            case 0x6:
                /* 01110101 00101011 0....... ........ */
                /* la.decode:2532 */
                return LISA_XVFRSTP_B;
            case 0x7:
                /* 01110101 00101011 1....... ........ */
                /* la.decode:2533 */
                return LISA_XVFRSTP_H;
            }
            return LISA_INVALID;
        case 0x4b:
            /* 01110101 001011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00101100 0....... ........ */
                /* la.decode:2534 */
                return LISA_XVCLRSTRR_V;
            case 0x1:
                /* 01110101 00101100 1....... ........ */
                /* la.decode:2535 */
                return LISA_XVCLRSTRV_V;
            case 0x2:
                /* 01110101 00101101 0....... ........ */
                /* la.decode:2536 */
                return LISA_XVADD_Q;
            case 0x3:
                /* 01110101 00101101 1....... ........ */
                /* la.decode:2537 */
                return LISA_XVSUB_Q;
            case 0x4:
                /* 01110101 00101110 0....... ........ */
                /* la.decode:2538 */
                return LISA_XVSIGNCOV_B;
            case 0x5:
                /* 01110101 00101110 1....... ........ */
                /* la.decode:2539 */
                return LISA_XVSIGNCOV_H;
            case 0x6:
                /* 01110101 00101111 0....... ........ */
                /* la.decode:2540 */
                return LISA_XVSIGNCOV_W;
            case 0x7:
                /* 01110101 00101111 1....... ........ */
                /* la.decode:2541 */
                return LISA_XVSIGNCOV_D;
            }
            return LISA_INVALID;
        case 0x4c:
            /* 01110101 001100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00110000 0....... ........ */
                /* la.decode:2542 */
                return LISA_XVFADD_H;
            case 0x1:
                /* 01110101 00110000 1....... ........ */
                /* la.decode:2543 */
                return LISA_XVFADD_S;
            case 0x2:
                /* 01110101 00110001 0....... ........ */
                /* la.decode:2544 */
                return LISA_XVFADD_D;
            case 0x3:
                /* 01110101 00110001 1....... ........ */
                /* la.decode:2545 */
                return LISA_XVFADD_Q;
            case 0x4:
                /* 01110101 00110010 0....... ........ */
                /* la.decode:2546 */
                return LISA_XVFSUB_H;
            case 0x5:
                /* 01110101 00110010 1....... ........ */
                /* la.decode:2547 */
                return LISA_XVFSUB_S;
            case 0x6:
                /* 01110101 00110011 0....... ........ */
                /* la.decode:2548 */
                return LISA_XVFSUB_D;
            case 0x7:
                /* 01110101 00110011 1....... ........ */
                /* la.decode:2549 */
                return LISA_XVFSUB_Q;
            }
            return LISA_INVALID;
        case 0x4d:
            /* 01110101 001101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00110100 0....... ........ */
                /* la.decode:2550 */
                return LISA_XVFADDSUB_H;
            case 0x1:
                /* 01110101 00110100 1....... ........ */
                /* la.decode:2551 */
                return LISA_XVFADDSUB_S;
            case 0x2:
                /* 01110101 00110101 0....... ........ */
                /* la.decode:2552 */
                return LISA_XVFADDSUB_D;
            case 0x3:
                /* 01110101 00110101 1....... ........ */
                /* la.decode:2553 */
                return LISA_XVFADDSUB_Q;
            case 0x4:
                /* 01110101 00110110 0....... ........ */
                /* la.decode:2554 */
                return LISA_XVFSUBADD_H;
            case 0x5:
                /* 01110101 00110110 1....... ........ */
                /* la.decode:2555 */
                return LISA_XVFSUBADD_S;
            case 0x6:
                /* 01110101 00110111 0....... ........ */
                /* la.decode:2556 */
                return LISA_XVFSUBADD_D;
            case 0x7:
                /* 01110101 00110111 1....... ........ */
                /* la.decode:2557 */
                return LISA_XVFSUBADD_Q;
            }
            return LISA_INVALID;
        case 0x4e:
            /* 01110101 001110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00111000 0....... ........ */
                /* la.decode:2558 */
                return LISA_XVFMUL_H;
            case 0x1:
                /* 01110101 00111000 1....... ........ */
                /* la.decode:2559 */
                return LISA_XVFMUL_S;
            case 0x2:
                /* 01110101 00111001 0....... ........ */
                /* la.decode:2560 */
                return LISA_XVFMUL_D;
            case 0x3:
                /* 01110101 00111001 1....... ........ */
                /* la.decode:2561 */
                return LISA_XVFMUL_Q;
            case 0x4:
                /* 01110101 00111010 0....... ........ */
                /* la.decode:2562 */
                return LISA_XVFDIV_H;
            case 0x5:
                /* 01110101 00111010 1....... ........ */
                /* la.decode:2563 */
                return LISA_XVFDIV_S;
            case 0x6:
                /* 01110101 00111011 0....... ........ */
                /* la.decode:2564 */
                return LISA_XVFDIV_D;
            case 0x7:
                /* 01110101 00111011 1....... ........ */
                /* la.decode:2565 */
                return LISA_XVFDIV_Q;
            }
            return LISA_INVALID;
        case 0x4f:
            /* 01110101 001111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 00111100 0....... ........ */
                /* la.decode:2566 */
                return LISA_XVFMAX_H;
            case 0x1:
                /* 01110101 00111100 1....... ........ */
                /* la.decode:2567 */
                return LISA_XVFMAX_S;
            case 0x2:
                /* 01110101 00111101 0....... ........ */
                /* la.decode:2568 */
                return LISA_XVFMAX_D;
            case 0x3:
                /* 01110101 00111101 1....... ........ */
                /* la.decode:2569 */
                return LISA_XVFMAX_Q;
            case 0x4:
                /* 01110101 00111110 0....... ........ */
                /* la.decode:2570 */
                return LISA_XVFMIN_H;
            case 0x5:
                /* 01110101 00111110 1....... ........ */
                /* la.decode:2571 */
                return LISA_XVFMIN_S;
            case 0x6:
                /* 01110101 00111111 0....... ........ */
                /* la.decode:2572 */
                return LISA_XVFMIN_D;
            case 0x7:
                /* 01110101 00111111 1....... ........ */
                /* la.decode:2573 */
                return LISA_XVFMIN_Q;
            }
            return LISA_INVALID;
        case 0x50:
            /* 01110101 010000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01000000 0....... ........ */
                /* la.decode:2574 */
                return LISA_XVFMAXA_H;
            case 0x1:
                /* 01110101 01000000 1....... ........ */
                /* la.decode:2575 */
                return LISA_XVFMAXA_S;
            case 0x2:
                /* 01110101 01000001 0....... ........ */
                /* la.decode:2576 */
                return LISA_XVFMAXA_D;
            case 0x3:
                /* 01110101 01000001 1....... ........ */
                /* la.decode:2577 */
                return LISA_XVFMAXA_Q;
            case 0x4:
                /* 01110101 01000010 0....... ........ */
                /* la.decode:2578 */
                return LISA_XVFMINA_H;
            case 0x5:
                /* 01110101 01000010 1....... ........ */
                /* la.decode:2579 */
                return LISA_XVFMINA_S;
            case 0x6:
                /* 01110101 01000011 0....... ........ */
                /* la.decode:2580 */
                return LISA_XVFMINA_D;
            case 0x7:
                /* 01110101 01000011 1....... ........ */
                /* la.decode:2581 */
                return LISA_XVFMINA_Q;
            }
            return LISA_INVALID;
        case 0x51:
            /* 01110101 010001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01000100 0....... ........ */
                /* la.decode:2582 */
                return LISA_XVFSCALEB_H;
            case 0x1:
                /* 01110101 01000100 1....... ........ */
                /* la.decode:2583 */
                return LISA_XVFSCALEB_S;
            case 0x2:
                /* 01110101 01000101 0....... ........ */
                /* la.decode:2584 */
                return LISA_XVFSCALEB_D;
            case 0x3:
                /* 01110101 01000101 1....... ........ */
                /* la.decode:2585 */
                return LISA_XVFSCALEB_Q;
            case 0x4:
                /* 01110101 01000110 0....... ........ */
                /* la.decode:2586 */
                return LISA_XVFCVT_H_S;
            case 0x5:
                /* 01110101 01000110 1....... ........ */
                /* la.decode:2587 */
                return LISA_XVFCVT_S_D;
            case 0x6:
                /* 01110101 01000111 0....... ........ */
                /* la.decode:2588 */
                return LISA_XVFCVT_D_Q;
            case 0x7:
                /* 01110101 01000111 1....... ........ */
                /* la.decode:2589 */
                return LISA_XVFFINT_H_W;
            }
            return LISA_INVALID;
        case 0x52:
            /* 01110101 010010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01001000 0....... ........ */
                /* la.decode:2590 */
                return LISA_XVFFINT_S_L;
            case 0x1:
                /* 01110101 01001000 1....... ........ */
                /* la.decode:2591 */
                return LISA_XVFFINT_H_WU;
            case 0x2:
                /* 01110101 01001001 0....... ........ */
                /* la.decode:2592 */
                return LISA_XVFFINT_S_LU;
            case 0x3:
                /* 01110101 01001001 1....... ........ */
                /* la.decode:2593 */
                return LISA_XVFTINT_W_D;
            case 0x4:
                /* 01110101 01001010 0....... ........ */
                /* la.decode:2594 */
                return LISA_XVFTINTRM_W_D;
            case 0x5:
                /* 01110101 01001010 1....... ........ */
                /* la.decode:2595 */
                return LISA_XVFTINTRP_W_D;
            case 0x6:
                /* 01110101 01001011 0....... ........ */
                /* la.decode:2596 */
                return LISA_XVFTINTRZ_W_D;
            case 0x7:
                /* 01110101 01001011 1....... ........ */
                /* la.decode:2597 */
                return LISA_XVFTINTRNE_W_D;
            }
            return LISA_INVALID;
        case 0x53:
            /* 01110101 010011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01001100 0....... ........ */
                /* la.decode:2598 */
                return LISA_XVFTINT_WU_D;
            case 0x1:
                /* 01110101 01001100 1....... ........ */
                /* la.decode:2599 */
                return LISA_XVFTINTRM_WU_D;
            case 0x2:
                /* 01110101 01001101 0....... ........ */
                /* la.decode:2600 */
                return LISA_XVFTINTRP_WU_D;
            case 0x3:
                /* 01110101 01001101 1....... ........ */
                /* la.decode:2601 */
                return LISA_XVFTINTRZ_WU_D;
            case 0x4:
                /* 01110101 01001110 0....... ........ */
                /* la.decode:2602 */
                return LISA_XVFTINTRNE_WU_D;
            case 0x5:
                /* 01110101 01001110 1....... ........ */
                /* la.decode:2603 */
                return LISA_XVHADD4_H_BU;
            case 0x6:
                /* 01110101 01001111 0....... ........ */
                /* la.decode:2604 */
                return LISA_XVSHUF4_W;
            case 0x7:
                /* 01110101 01001111 1....... ........ */
                /* la.decode:2605 */
                return LISA_XVSHUF2_D;
            }
            return LISA_INVALID;
        case 0x58:
            /* 01110101 011000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01100000 0....... ........ */
                /* la.decode:2606 */
                return LISA_XVPMUL_W;
            case 0x1:
                /* 01110101 01100000 1....... ........ */
                /* la.decode:2607 */
                return LISA_XVPMUL_D;
            case 0x2:
                /* 01110101 01100001 0....... ........ */
                /* la.decode:2608 */
                return LISA_XVPMUH_W;
            case 0x3:
                /* 01110101 01100001 1....... ........ */
                /* la.decode:2609 */
                return LISA_XVPMUH_D;
            case 0x4:
                /* 01110101 01100010 0....... ........ */
                /* la.decode:2610 */
                return LISA_XVPMULACC_W;
            case 0x5:
                /* 01110101 01100010 1....... ........ */
                /* la.decode:2611 */
                return LISA_XVPMULACC_D;
            case 0x6:
                /* 01110101 01100011 0....... ........ */
                /* la.decode:2612 */
                return LISA_XVPMUHACC_W;
            case 0x7:
                /* 01110101 01100011 1....... ........ */
                /* la.decode:2613 */
                return LISA_XVPMUHACC_D;
            }
            return LISA_INVALID;
        case 0x59:
            /* 01110101 011001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01100100 0....... ........ */
                /* la.decode:2614 */
                return LISA_XVPMULWL_H_B;
            case 0x1:
                /* 01110101 01100100 1....... ........ */
                /* la.decode:2615 */
                return LISA_XVPMULWL_W_H;
            case 0x2:
                /* 01110101 01100101 0....... ........ */
                /* la.decode:2616 */
                return LISA_XVPMULWL_D_W;
            case 0x3:
                /* 01110101 01100101 1....... ........ */
                /* la.decode:2617 */
                return LISA_XVPMULWL_Q_D;
            case 0x4:
                /* 01110101 01100110 0....... ........ */
                /* la.decode:2618 */
                return LISA_XVPMULWH_H_B;
            case 0x5:
                /* 01110101 01100110 1....... ........ */
                /* la.decode:2619 */
                return LISA_XVPMULWH_W_H;
            case 0x6:
                /* 01110101 01100111 0....... ........ */
                /* la.decode:2620 */
                return LISA_XVPMULWH_D_W;
            case 0x7:
                /* 01110101 01100111 1....... ........ */
                /* la.decode:2621 */
                return LISA_XVPMULWH_Q_D;
            }
            return LISA_INVALID;
        case 0x5a:
            /* 01110101 011010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01101000 0....... ........ */
                /* la.decode:2622 */
                return LISA_XVPMADDWL_H_B;
            case 0x1:
                /* 01110101 01101000 1....... ........ */
                /* la.decode:2623 */
                return LISA_XVPMADDWL_W_H;
            case 0x2:
                /* 01110101 01101001 0....... ........ */
                /* la.decode:2624 */
                return LISA_XVPMADDWL_D_W;
            case 0x3:
                /* 01110101 01101001 1....... ........ */
                /* la.decode:2625 */
                return LISA_XVPMADDWL_Q_D;
            case 0x4:
                /* 01110101 01101010 0....... ........ */
                /* la.decode:2626 */
                return LISA_XVPMADDWH_H_B;
            case 0x5:
                /* 01110101 01101010 1....... ........ */
                /* la.decode:2627 */
                return LISA_XVPMADDWH_W_H;
            case 0x6:
                /* 01110101 01101011 0....... ........ */
                /* la.decode:2628 */
                return LISA_XVPMADDWH_D_W;
            case 0x7:
                /* 01110101 01101011 1....... ........ */
                /* la.decode:2629 */
                return LISA_XVPMADDWH_Q_D;
            }
            return LISA_INVALID;
        case 0x5b:
            /* 01110101 011011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01101100 0....... ........ */
                /* la.decode:2630 */
                return LISA_XVPDP2_Q_D;
            case 0x1:
                /* 01110101 01101100 1....... ........ */
                /* la.decode:2631 */
                return LISA_XVPDP2ADD_Q_D;
            }
            return LISA_INVALID;
        case 0x5c:
            /* 01110101 011100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110101 01110000 0....... ........ */
                /* la.decode:2632 */
                return LISA_XVCDP4_RE_D_H;
            case 0x1:
                /* 01110101 01110000 1....... ........ */
                /* la.decode:2633 */
                return LISA_XVCDP4_IM_D_H;
            case 0x2:
                /* 01110101 01110001 0....... ........ */
                /* la.decode:2634 */
                return LISA_XVCDP4ADD_RE_D_H;
            case 0x3:
                /* 01110101 01110001 1....... ........ */
                /* la.decode:2635 */
                return LISA_XVCDP4ADD_IM_D_H;
            case 0x4:
                /* 01110101 01110010 0....... ........ */
                /* la.decode:2636 */
                return LISA_XVCDP2_RE_Q_W;
            case 0x5:
                /* 01110101 01110010 1....... ........ */
                /* la.decode:2637 */
                return LISA_XVCDP2_IM_Q_W;
            case 0x6:
                /* 01110101 01110011 0....... ........ */
                /* la.decode:2638 */
                return LISA_XVCDP2ADD_RE_Q_W;
            case 0x7:
                /* 01110101 01110011 1....... ........ */
                /* la.decode:2639 */
                return LISA_XVCDP2ADD_IM_Q_W;
            }
            return LISA_INVALID;
        case 0x5e:
            /* 01110101 011110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x2:
                /* 01110101 01111001 0....... ........ */
                /* la.decode:2640 */
                return LISA_XVSIGNSEL_W;
            case 0x3:
                /* 01110101 01111001 1....... ........ */
                /* la.decode:2641 */
                return LISA_XVSIGNSEL_D;
            case 0x5:
                /* 01110101 01111010 1....... ........ */
                /* la.decode:2642 */
                return LISA_XVSHUF_H;
            case 0x6:
                /* 01110101 01111011 0....... ........ */
                /* la.decode:2643 */
                return LISA_XVSHUF_W;
            case 0x7:
                /* 01110101 01111011 1....... ........ */
                /* la.decode:2644 */
                return LISA_XVSHUF_D;
            }
            return LISA_INVALID;
        case 0x5f:
            /* 01110101 011111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x2:
                /* 01110101 01111101 0....... ........ */
                /* la.decode:2645 */
                return LISA_XVPERM_W;
            }
            return LISA_INVALID;
        case 0xa0:
            /* 01110110 100000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10000000 0....... ........ */
                /* la.decode:2646 */
                return LISA_XVSEQI_B;
            case 0x1:
                /* 01110110 10000000 1....... ........ */
                /* la.decode:2647 */
                return LISA_XVSEQI_H;
            case 0x2:
                /* 01110110 10000001 0....... ........ */
                /* la.decode:2648 */
                return LISA_XVSEQI_W;
            case 0x3:
                /* 01110110 10000001 1....... ........ */
                /* la.decode:2649 */
                return LISA_XVSEQI_D;
            case 0x4:
                /* 01110110 10000010 0....... ........ */
                /* la.decode:2650 */
                return LISA_XVSLEI_B;
            case 0x5:
                /* 01110110 10000010 1....... ........ */
                /* la.decode:2651 */
                return LISA_XVSLEI_H;
            case 0x6:
                /* 01110110 10000011 0....... ........ */
                /* la.decode:2652 */
                return LISA_XVSLEI_W;
            case 0x7:
                /* 01110110 10000011 1....... ........ */
                /* la.decode:2653 */
                return LISA_XVSLEI_D;
            }
            return LISA_INVALID;
        case 0xa1:
            /* 01110110 100001.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10000100 0....... ........ */
                /* la.decode:2654 */
                return LISA_XVSLEI_BU;
            case 0x1:
                /* 01110110 10000100 1....... ........ */
                /* la.decode:2655 */
                return LISA_XVSLEI_HU;
            case 0x2:
                /* 01110110 10000101 0....... ........ */
                /* la.decode:2656 */
                return LISA_XVSLEI_WU;
            case 0x3:
                /* 01110110 10000101 1....... ........ */
                /* la.decode:2657 */
                return LISA_XVSLEI_DU;
            case 0x4:
                /* 01110110 10000110 0....... ........ */
                /* la.decode:2658 */
                return LISA_XVSLTI_B;
            case 0x5:
                /* 01110110 10000110 1....... ........ */
                /* la.decode:2659 */
                return LISA_XVSLTI_H;
            case 0x6:
                /* 01110110 10000111 0....... ........ */
                /* la.decode:2660 */
                return LISA_XVSLTI_W;
            case 0x7:
                /* 01110110 10000111 1....... ........ */
                /* la.decode:2661 */
                return LISA_XVSLTI_D;
            }
            return LISA_INVALID;
        case 0xa2:
            /* 01110110 100010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10001000 0....... ........ */
                /* la.decode:2662 */
                return LISA_XVSLTI_BU;
            case 0x1:
                /* 01110110 10001000 1....... ........ */
                /* la.decode:2663 */
                return LISA_XVSLTI_HU;
            case 0x2:
                /* 01110110 10001001 0....... ........ */
                /* la.decode:2664 */
                return LISA_XVSLTI_WU;
            case 0x3:
                /* 01110110 10001001 1....... ........ */
                /* la.decode:2665 */
                return LISA_XVSLTI_DU;
            case 0x4:
                /* 01110110 10001010 0....... ........ */
                /* la.decode:2666 */
                return LISA_XVADDI_BU;
            case 0x5:
                /* 01110110 10001010 1....... ........ */
                /* la.decode:2667 */
                return LISA_XVADDI_HU;
            case 0x6:
                /* 01110110 10001011 0....... ........ */
                /* la.decode:2668 */
                return LISA_XVADDI_WU;
            case 0x7:
                /* 01110110 10001011 1....... ........ */
                /* la.decode:2669 */
                return LISA_XVADDI_DU;
            }
            return LISA_INVALID;
        case 0xa3:
            /* 01110110 100011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10001100 0....... ........ */
                /* la.decode:2670 */
                return LISA_XVSUBI_BU;
            case 0x1:
                /* 01110110 10001100 1....... ........ */
                /* la.decode:2671 */
                return LISA_XVSUBI_HU;
            case 0x2:
                /* 01110110 10001101 0....... ........ */
                /* la.decode:2672 */
                return LISA_XVSUBI_WU;
            case 0x3:
                /* 01110110 10001101 1....... ........ */
                /* la.decode:2673 */
                return LISA_XVSUBI_DU;
            case 0x4:
                /* 01110110 10001110 0....... ........ */
                /* la.decode:2674 */
                return LISA_XVBSLL_V;
            case 0x5:
                /* 01110110 10001110 1....... ........ */
                /* la.decode:2675 */
                return LISA_XVBSRL_V;
            }
            return LISA_INVALID;
        case 0xa4:
            /* 01110110 100100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10010000 0....... ........ */
                /* la.decode:2676 */
                return LISA_XVMAXI_B;
            case 0x1:
                /* 01110110 10010000 1....... ........ */
                /* la.decode:2677 */
                return LISA_XVMAXI_H;
            case 0x2:
                /* 01110110 10010001 0....... ........ */
                /* la.decode:2678 */
                return LISA_XVMAXI_W;
            case 0x3:
                /* 01110110 10010001 1....... ........ */
                /* la.decode:2679 */
                return LISA_XVMAXI_D;
            case 0x4:
                /* 01110110 10010010 0....... ........ */
                /* la.decode:2680 */
                return LISA_XVMINI_B;
            case 0x5:
                /* 01110110 10010010 1....... ........ */
                /* la.decode:2681 */
                return LISA_XVMINI_H;
            case 0x6:
                /* 01110110 10010011 0....... ........ */
                /* la.decode:2682 */
                return LISA_XVMINI_W;
            case 0x7:
                /* 01110110 10010011 1....... ........ */
                /* la.decode:2683 */
                return LISA_XVMINI_D;
            }
            return LISA_INVALID;
        case 0xa5:
            /* 01110110 100101.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10010100 0....... ........ */
                /* la.decode:2684 */
                return LISA_XVMAXI_BU;
            case 0x1:
                /* 01110110 10010100 1....... ........ */
                /* la.decode:2685 */
                return LISA_XVMAXI_HU;
            case 0x2:
                /* 01110110 10010101 0....... ........ */
                /* la.decode:2686 */
                return LISA_XVMAXI_WU;
            case 0x3:
                /* 01110110 10010101 1....... ........ */
                /* la.decode:2687 */
                return LISA_XVMAXI_DU;
            case 0x4:
                /* 01110110 10010110 0....... ........ */
                /* la.decode:2688 */
                return LISA_XVMINI_BU;
            case 0x5:
                /* 01110110 10010110 1....... ........ */
                /* la.decode:2689 */
                return LISA_XVMINI_HU;
            case 0x6:
                /* 01110110 10010111 0....... ........ */
                /* la.decode:2690 */
                return LISA_XVMINI_WU;
            case 0x7:
                /* 01110110 10010111 1....... ........ */
                /* la.decode:2691 */
                return LISA_XVMINI_DU;
            }
            return LISA_INVALID;
        case 0xa6:
            /* 01110110 100110.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10011000 0....... ........ */
                /* la.decode:2692 */
                return LISA_XVRANDSIGNI_B;
            case 0x1:
                /* 01110110 10011000 1....... ........ */
                /* la.decode:2693 */
                return LISA_XVRANDSIGNI_H;
            case 0x2:
                /* 01110110 10011001 0....... ........ */
                /* la.decode:2694 */
                return LISA_XVRORSIGNI_B;
            case 0x3:
                /* 01110110 10011001 1....... ........ */
                /* la.decode:2695 */
                return LISA_XVRORSIGNI_H;
            case 0x4:
                /* 01110110 10011010 0....... ........ */
                /* la.decode:2696 */
                return LISA_XVFRSTPI_B;
            case 0x5:
                /* 01110110 10011010 1....... ........ */
                /* la.decode:2697 */
                return LISA_XVFRSTPI_H;
            case 0x6:
                /* 01110110 10011011 0....... ........ */
                /* la.decode:2698 */
                return LISA_XVCLRSTRI_V;
            case 0x7:
                /* 01110110 10011011 1....... ........ */
                /* la.decode:2699 */
                return LISA_XVMEPATMSK_V;
            }
            return LISA_INVALID;
        case 0xa7:
            /* 01110110 100111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x0:
                /* 01110110 10011100 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011100 000000.. ........ */
                    /* la.decode:2700 */
                    return LISA_XVCLO_B;
                case 0x1:
                    /* 01110110 10011100 000001.. ........ */
                    /* la.decode:2701 */
                    return LISA_XVCLO_H;
                case 0x2:
                    /* 01110110 10011100 000010.. ........ */
                    /* la.decode:2702 */
                    return LISA_XVCLO_W;
                case 0x3:
                    /* 01110110 10011100 000011.. ........ */
                    /* la.decode:2703 */
                    return LISA_XVCLO_D;
                case 0x4:
                    /* 01110110 10011100 000100.. ........ */
                    /* la.decode:2704 */
                    return LISA_XVCLZ_B;
                case 0x5:
                    /* 01110110 10011100 000101.. ........ */
                    /* la.decode:2705 */
                    return LISA_XVCLZ_H;
                case 0x6:
                    /* 01110110 10011100 000110.. ........ */
                    /* la.decode:2706 */
                    return LISA_XVCLZ_W;
                case 0x7:
                    /* 01110110 10011100 000111.. ........ */
                    /* la.decode:2707 */
                    return LISA_XVCLZ_D;
                case 0x8:
                    /* 01110110 10011100 001000.. ........ */
                    /* la.decode:2708 */
                    return LISA_XVPCNT_B;
                case 0x9:
                    /* 01110110 10011100 001001.. ........ */
                    /* la.decode:2709 */
                    return LISA_XVPCNT_H;
                case 0xa:
                    /* 01110110 10011100 001010.. ........ */
                    /* la.decode:2710 */
                    return LISA_XVPCNT_W;
                case 0xb:
                    /* 01110110 10011100 001011.. ........ */
                    /* la.decode:2711 */
                    return LISA_XVPCNT_D;
                case 0xc:
                    /* 01110110 10011100 001100.. ........ */
                    /* la.decode:2712 */
                    return LISA_XVNEG_B;
                case 0xd:
                    /* 01110110 10011100 001101.. ........ */
                    /* la.decode:2713 */
                    return LISA_XVNEG_H;
                case 0xe:
                    /* 01110110 10011100 001110.. ........ */
                    /* la.decode:2714 */
                    return LISA_XVNEG_W;
                case 0xf:
                    /* 01110110 10011100 001111.. ........ */
                    /* la.decode:2715 */
                    return LISA_XVNEG_D;
                case 0x10:
                    /* 01110110 10011100 010000.. ........ */
                    /* la.decode:2716 */
                    return LISA_XVMSKLTZ_B;
                case 0x11:
                    /* 01110110 10011100 010001.. ........ */
                    /* la.decode:2717 */
                    return LISA_XVMSKLTZ_H;
                case 0x12:
                    /* 01110110 10011100 010010.. ........ */
                    /* la.decode:2718 */
                    return LISA_XVMSKLTZ_W;
                case 0x13:
                    /* 01110110 10011100 010011.. ........ */
                    /* la.decode:2719 */
                    return LISA_XVMSKLTZ_D;
                case 0x14:
                    /* 01110110 10011100 010100.. ........ */
                    /* la.decode:2720 */
                    return LISA_XVMSKGEZ_B;
                case 0x15:
                    /* 01110110 10011100 010101.. ........ */
                    /* la.decode:2721 */
                    return LISA_XVMSKGEZ_H;
                case 0x16:
                    /* 01110110 10011100 010110.. ........ */
                    /* la.decode:2722 */
                    return LISA_XVMSKGEZ_W;
                case 0x17:
                    /* 01110110 10011100 010111.. ........ */
                    /* la.decode:2723 */
                    return LISA_XVMSKGEZ_D;
                case 0x18:
                    /* 01110110 10011100 011000.. ........ */
                    /* la.decode:2724 */
                    return LISA_XVMSKNZ_B;
                case 0x19:
                    /* 01110110 10011100 011001.. ........ */
                    /* la.decode:2725 */
                    return LISA_XVMSKNZ_H;
                case 0x1a:
                    /* 01110110 10011100 011010.. ........ */
                    /* la.decode:2726 */
                    return LISA_XVMSKNZ_W;
                case 0x1b:
                    /* 01110110 10011100 011011.. ........ */
                    /* la.decode:2727 */
                    return LISA_XVMSKNZ_D;
                case 0x1c:
                    /* 01110110 10011100 011100.. ........ */
                    /* la.decode:2728 */
                    return LISA_XVMSKCOPY_B;
                case 0x1d:
                    /* 01110110 10011100 011101.. ........ */
                    /* la.decode:2729 */
                    return LISA_XVMSKCOPY_H;
                case 0x1e:
                    /* 01110110 10011100 011110.. ........ */
                    /* la.decode:2730 */
                    return LISA_XVMSKCOPY_W;
                case 0x1f:
                    /* 01110110 10011100 011111.. ........ */
                    /* la.decode:2731 */
                    return LISA_XVMSKCOPY_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110110 10011100 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011100 100000.. ........ */
                    /* la.decode:2732 */
                    return LISA_XVMSKFILL_B;
                case 0x1:
                    /* 01110110 10011100 100001.. ........ */
                    /* la.decode:2733 */
                    return LISA_XVMSKFILL_H;
                case 0x2:
                    /* 01110110 10011100 100010.. ........ */
                    /* la.decode:2734 */
                    return LISA_XVMSKFILL_W;
                case 0x3:
                    /* 01110110 10011100 100011.. ........ */
                    /* la.decode:2735 */
                    return LISA_XVMSKFILL_D;
                case 0x4:
                    /* 01110110 10011100 100100.. ........ */
                    /* la.decode:2736 */
                    return LISA_XVFRSTM_B;
                case 0x5:
                    /* 01110110 10011100 100101.. ........ */
                    /* la.decode:2737 */
                    return LISA_XVFRSTM_H;
                case 0x6:
                    /* 01110110 10011100 100110.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 100110.. ...00... */
                        /* la.decode:2738 */
                        return LISA_XVSETEQZ_V;
                    }
                    return LISA_INVALID;
                case 0x7:
                    /* 01110110 10011100 100111.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 100111.. ...00... */
                        /* la.decode:2739 */
                        return LISA_XVSETNEZ_V;
                    }
                    return LISA_INVALID;
                case 0x8:
                    /* 01110110 10011100 101000.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101000.. ...00... */
                        /* la.decode:2740 */
                        return LISA_XVSETANYEQZ_B;
                    }
                    return LISA_INVALID;
                case 0x9:
                    /* 01110110 10011100 101001.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101001.. ...00... */
                        /* la.decode:2741 */
                        return LISA_XVSETANYEQZ_H;
                    }
                    return LISA_INVALID;
                case 0xa:
                    /* 01110110 10011100 101010.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101010.. ...00... */
                        /* la.decode:2742 */
                        return LISA_XVSETANYEQZ_W;
                    }
                    return LISA_INVALID;
                case 0xb:
                    /* 01110110 10011100 101011.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101011.. ...00... */
                        /* la.decode:2743 */
                        return LISA_XVSETANYEQZ_D;
                    }
                    return LISA_INVALID;
                case 0xc:
                    /* 01110110 10011100 101100.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101100.. ...00... */
                        /* la.decode:2744 */
                        return LISA_XVSETALLNEZ_B;
                    }
                    return LISA_INVALID;
                case 0xd:
                    /* 01110110 10011100 101101.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101101.. ...00... */
                        /* la.decode:2745 */
                        return LISA_XVSETALLNEZ_H;
                    }
                    return LISA_INVALID;
                case 0xe:
                    /* 01110110 10011100 101110.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101110.. ...00... */
                        /* la.decode:2746 */
                        return LISA_XVSETALLNEZ_W;
                    }
                    return LISA_INVALID;
                case 0xf:
                    /* 01110110 10011100 101111.. ........ */
                    switch ((insn >> 3) & 0x3) {
                    case 0x0:
                        /* 01110110 10011100 101111.. ...00... */
                        /* la.decode:2747 */
                        return LISA_XVSETALLNEZ_D;
                    }
                    return LISA_INVALID;
                case 0x10:
                    /* 01110110 10011100 110000.. ........ */
                    /* la.decode:2748 */
                    return LISA_XVFLOGB_H;
                case 0x11:
                    /* 01110110 10011100 110001.. ........ */
                    /* la.decode:2749 */
                    return LISA_XVFLOGB_S;
                case 0x12:
                    /* 01110110 10011100 110010.. ........ */
                    /* la.decode:2750 */
                    return LISA_XVFLOGB_D;
                case 0x13:
                    /* 01110110 10011100 110011.. ........ */
                    /* la.decode:2751 */
                    return LISA_XVFLOGB_Q;
                case 0x14:
                    /* 01110110 10011100 110100.. ........ */
                    /* la.decode:2752 */
                    return LISA_XVFCLASS_H;
                case 0x15:
                    /* 01110110 10011100 110101.. ........ */
                    /* la.decode:2753 */
                    return LISA_XVFCLASS_S;
                case 0x16:
                    /* 01110110 10011100 110110.. ........ */
                    /* la.decode:2754 */
                    return LISA_XVFCLASS_D;
                case 0x17:
                    /* 01110110 10011100 110111.. ........ */
                    /* la.decode:2755 */
                    return LISA_XVFCLASS_Q;
                case 0x18:
                    /* 01110110 10011100 111000.. ........ */
                    /* la.decode:2756 */
                    return LISA_XVFSQRT_H;
                case 0x19:
                    /* 01110110 10011100 111001.. ........ */
                    /* la.decode:2757 */
                    return LISA_XVFSQRT_S;
                case 0x1a:
                    /* 01110110 10011100 111010.. ........ */
                    /* la.decode:2758 */
                    return LISA_XVFSQRT_D;
                case 0x1b:
                    /* 01110110 10011100 111011.. ........ */
                    /* la.decode:2759 */
                    return LISA_XVFSQRT_Q;
                case 0x1c:
                    /* 01110110 10011100 111100.. ........ */
                    /* la.decode:2760 */
                    return LISA_XVFRECIP_H;
                case 0x1d:
                    /* 01110110 10011100 111101.. ........ */
                    /* la.decode:2761 */
                    return LISA_XVFRECIP_S;
                case 0x1e:
                    /* 01110110 10011100 111110.. ........ */
                    /* la.decode:2762 */
                    return LISA_XVFRECIP_D;
                case 0x1f:
                    /* 01110110 10011100 111111.. ........ */
                    /* la.decode:2763 */
                    return LISA_XVFRECIP_Q;
                }
                return LISA_INVALID;
            case 0x2:
                /* 01110110 10011101 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011101 000000.. ........ */
                    /* la.decode:2764 */
                    return LISA_XVFRSQRT_H;
                case 0x1:
                    /* 01110110 10011101 000001.. ........ */
                    /* la.decode:2765 */
                    return LISA_XVFRSQRT_S;
                case 0x2:
                    /* 01110110 10011101 000010.. ........ */
                    /* la.decode:2766 */
                    return LISA_XVFRSQRT_D;
                case 0x3:
                    /* 01110110 10011101 000011.. ........ */
                    /* la.decode:2767 */
                    return LISA_XVFRSQRT_Q;
                case 0x4:
                    /* 01110110 10011101 000100.. ........ */
                    /* la.decode:2768 */
                    return LISA_XVFRECIPE_H;
                case 0x5:
                    /* 01110110 10011101 000101.. ........ */
                    /* la.decode:2769 */
                    return LISA_XVFRECIPE_S;
                case 0x6:
                    /* 01110110 10011101 000110.. ........ */
                    /* la.decode:2770 */
                    return LISA_XVFRECIPE_D;
                case 0x7:
                    /* 01110110 10011101 000111.. ........ */
                    /* la.decode:2771 */
                    return LISA_XVFRECIPE_Q;
                case 0x8:
                    /* 01110110 10011101 001000.. ........ */
                    /* la.decode:2772 */
                    return LISA_XVFRSQRTE_H;
                case 0x9:
                    /* 01110110 10011101 001001.. ........ */
                    /* la.decode:2773 */
                    return LISA_XVFRSQRTE_S;
                case 0xa:
                    /* 01110110 10011101 001010.. ........ */
                    /* la.decode:2774 */
                    return LISA_XVFRSQRTE_D;
                case 0xb:
                    /* 01110110 10011101 001011.. ........ */
                    /* la.decode:2775 */
                    return LISA_XVFRSQRTE_Q;
                case 0xc:
                    /* 01110110 10011101 001100.. ........ */
                    /* la.decode:2776 */
                    return LISA_XVFRINT_H;
                case 0xd:
                    /* 01110110 10011101 001101.. ........ */
                    /* la.decode:2777 */
                    return LISA_XVFRINT_S;
                case 0xe:
                    /* 01110110 10011101 001110.. ........ */
                    /* la.decode:2778 */
                    return LISA_XVFRINT_D;
                case 0xf:
                    /* 01110110 10011101 001111.. ........ */
                    /* la.decode:2779 */
                    return LISA_XVFRINT_Q;
                case 0x10:
                    /* 01110110 10011101 010000.. ........ */
                    /* la.decode:2780 */
                    return LISA_XVFRINTRM_H;
                case 0x11:
                    /* 01110110 10011101 010001.. ........ */
                    /* la.decode:2781 */
                    return LISA_XVFRINTRM_S;
                case 0x12:
                    /* 01110110 10011101 010010.. ........ */
                    /* la.decode:2782 */
                    return LISA_XVFRINTRM_D;
                case 0x13:
                    /* 01110110 10011101 010011.. ........ */
                    /* la.decode:2783 */
                    return LISA_XVFRINTRM_Q;
                case 0x14:
                    /* 01110110 10011101 010100.. ........ */
                    /* la.decode:2784 */
                    return LISA_XVFRINTRP_H;
                case 0x15:
                    /* 01110110 10011101 010101.. ........ */
                    /* la.decode:2785 */
                    return LISA_XVFRINTRP_S;
                case 0x16:
                    /* 01110110 10011101 010110.. ........ */
                    /* la.decode:2786 */
                    return LISA_XVFRINTRP_D;
                case 0x17:
                    /* 01110110 10011101 010111.. ........ */
                    /* la.decode:2787 */
                    return LISA_XVFRINTRP_Q;
                case 0x18:
                    /* 01110110 10011101 011000.. ........ */
                    /* la.decode:2788 */
                    return LISA_XVFRINTRZ_H;
                case 0x19:
                    /* 01110110 10011101 011001.. ........ */
                    /* la.decode:2789 */
                    return LISA_XVFRINTRZ_S;
                case 0x1a:
                    /* 01110110 10011101 011010.. ........ */
                    /* la.decode:2790 */
                    return LISA_XVFRINTRZ_D;
                case 0x1b:
                    /* 01110110 10011101 011011.. ........ */
                    /* la.decode:2791 */
                    return LISA_XVFRINTRZ_Q;
                case 0x1c:
                    /* 01110110 10011101 011100.. ........ */
                    /* la.decode:2792 */
                    return LISA_XVFRINTRNE_H;
                case 0x1d:
                    /* 01110110 10011101 011101.. ........ */
                    /* la.decode:2793 */
                    return LISA_XVFRINTRNE_S;
                case 0x1e:
                    /* 01110110 10011101 011110.. ........ */
                    /* la.decode:2794 */
                    return LISA_XVFRINTRNE_D;
                case 0x1f:
                    /* 01110110 10011101 011111.. ........ */
                    /* la.decode:2795 */
                    return LISA_XVFRINTRNE_Q;
                }
                return LISA_INVALID;
            case 0x3:
                /* 01110110 10011101 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x1:
                    /* 01110110 10011101 100001.. ........ */
                    /* la.decode:2796 */
                    return LISA_XVEXTL_W_B;
                case 0x2:
                    /* 01110110 10011101 100010.. ........ */
                    /* la.decode:2797 */
                    return LISA_XVEXTL_D_B;
                case 0x3:
                    /* 01110110 10011101 100011.. ........ */
                    /* la.decode:2798 */
                    return LISA_XVEXTL_Q_B;
                case 0x5:
                    /* 01110110 10011101 100101.. ........ */
                    /* la.decode:2799 */
                    return LISA_XVEXTL_D_H;
                case 0x6:
                    /* 01110110 10011101 100110.. ........ */
                    /* la.decode:2800 */
                    return LISA_XVEXTL_Q_H;
                case 0x8:
                    /* 01110110 10011101 101000.. ........ */
                    /* la.decode:2801 */
                    return LISA_XVEXTL_Q_W;
                case 0xb:
                    /* 01110110 10011101 101011.. ........ */
                    /* la.decode:2802 */
                    return LISA_XVEXTL_W_BU;
                case 0xc:
                    /* 01110110 10011101 101100.. ........ */
                    /* la.decode:2803 */
                    return LISA_XVEXTL_D_BU;
                case 0xd:
                    /* 01110110 10011101 101101.. ........ */
                    /* la.decode:2804 */
                    return LISA_XVEXTL_Q_BU;
                case 0xf:
                    /* 01110110 10011101 101111.. ........ */
                    /* la.decode:2805 */
                    return LISA_XVEXTL_D_HU;
                case 0x10:
                    /* 01110110 10011101 110000.. ........ */
                    /* la.decode:2806 */
                    return LISA_XVEXTL_Q_HU;
                case 0x12:
                    /* 01110110 10011101 110010.. ........ */
                    /* la.decode:2807 */
                    return LISA_XVEXTL_Q_WU;
                case 0x14:
                    /* 01110110 10011101 110100.. ........ */
                    /* la.decode:2808 */
                    return LISA_XVHADD8_D_BU;
                case 0x15:
                    /* 01110110 10011101 110101.. ........ */
                    /* la.decode:2809 */
                    return LISA_XVHMINPOS_W_HU;
                case 0x16:
                    /* 01110110 10011101 110110.. ........ */
                    /* la.decode:2810 */
                    return LISA_XVHMINPOS_D_HU;
                case 0x17:
                    /* 01110110 10011101 110111.. ........ */
                    /* la.decode:2811 */
                    return LISA_XVHMINPOS_Q_HU;
                case 0x18:
                    /* 01110110 10011101 111000.. ........ */
                    /* la.decode:2812 */
                    return LISA_XVCLRTAIL_B;
                case 0x19:
                    /* 01110110 10011101 111001.. ........ */
                    /* la.decode:2813 */
                    return LISA_XVCLRTAIL_H;
                case 0x1a:
                    /* 01110110 10011101 111010.. ........ */
                    /* la.decode:2814 */
                    return LISA_XVFCVTL_S_H;
                case 0x1b:
                    /* 01110110 10011101 111011.. ........ */
                    /* la.decode:2815 */
                    return LISA_XVFCVTH_S_H;
                case 0x1c:
                    /* 01110110 10011101 111100.. ........ */
                    /* la.decode:2816 */
                    return LISA_XVFCVTL_D_S;
                case 0x1d:
                    /* 01110110 10011101 111101.. ........ */
                    /* la.decode:2817 */
                    return LISA_XVFCVTH_D_S;
                case 0x1e:
                    /* 01110110 10011101 111110.. ........ */
                    /* la.decode:2818 */
                    return LISA_XVFCVTL_Q_D;
                case 0x1f:
                    /* 01110110 10011101 111111.. ........ */
                    /* la.decode:2819 */
                    return LISA_XVFCVTH_Q_D;
                }
                return LISA_INVALID;
            case 0x4:
                /* 01110110 10011110 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011110 000000.. ........ */
                    /* la.decode:2820 */
                    return LISA_XVFFINT_S_W;
                case 0x1:
                    /* 01110110 10011110 000001.. ........ */
                    /* la.decode:2821 */
                    return LISA_XVFFINT_S_WU;
                case 0x2:
                    /* 01110110 10011110 000010.. ........ */
                    /* la.decode:2822 */
                    return LISA_XVFFINT_D_L;
                case 0x3:
                    /* 01110110 10011110 000011.. ........ */
                    /* la.decode:2823 */
                    return LISA_XVFFINT_D_LU;
                case 0x4:
                    /* 01110110 10011110 000100.. ........ */
                    /* la.decode:2824 */
                    return LISA_XVFFINTL_D_W;
                case 0x5:
                    /* 01110110 10011110 000101.. ........ */
                    /* la.decode:2825 */
                    return LISA_XVFFINTH_D_W;
                case 0x6:
                    /* 01110110 10011110 000110.. ........ */
                    /* la.decode:2826 */
                    return LISA_XVFFINTL_Q_L;
                case 0x7:
                    /* 01110110 10011110 000111.. ........ */
                    /* la.decode:2827 */
                    return LISA_XVFFINTH_Q_L;
                case 0x8:
                    /* 01110110 10011110 001000.. ........ */
                    /* la.decode:2828 */
                    return LISA_XVFFINTL_D_WU;
                case 0x9:
                    /* 01110110 10011110 001001.. ........ */
                    /* la.decode:2829 */
                    return LISA_XVFFINTH_D_WU;
                case 0xa:
                    /* 01110110 10011110 001010.. ........ */
                    /* la.decode:2830 */
                    return LISA_XVFFINTL_Q_LU;
                case 0xb:
                    /* 01110110 10011110 001011.. ........ */
                    /* la.decode:2831 */
                    return LISA_XVFFINTH_Q_LU;
                case 0xc:
                    /* 01110110 10011110 001100.. ........ */
                    /* la.decode:2832 */
                    return LISA_XVFTINT_W_S;
                case 0xd:
                    /* 01110110 10011110 001101.. ........ */
                    /* la.decode:2833 */
                    return LISA_XVFTINT_L_D;
                case 0xe:
                    /* 01110110 10011110 001110.. ........ */
                    /* la.decode:2834 */
                    return LISA_XVFTINTRM_W_S;
                case 0xf:
                    /* 01110110 10011110 001111.. ........ */
                    /* la.decode:2835 */
                    return LISA_XVFTINTRM_L_D;
                case 0x10:
                    /* 01110110 10011110 010000.. ........ */
                    /* la.decode:2836 */
                    return LISA_XVFTINTRP_W_S;
                case 0x11:
                    /* 01110110 10011110 010001.. ........ */
                    /* la.decode:2837 */
                    return LISA_XVFTINTRP_L_D;
                case 0x12:
                    /* 01110110 10011110 010010.. ........ */
                    /* la.decode:2838 */
                    return LISA_XVFTINTRZ_W_S;
                case 0x13:
                    /* 01110110 10011110 010011.. ........ */
                    /* la.decode:2839 */
                    return LISA_XVFTINTRZ_L_D;
                case 0x14:
                    /* 01110110 10011110 010100.. ........ */
                    /* la.decode:2840 */
                    return LISA_XVFTINTRNE_W_S;
                case 0x15:
                    /* 01110110 10011110 010101.. ........ */
                    /* la.decode:2841 */
                    return LISA_XVFTINTRNE_L_D;
                case 0x16:
                    /* 01110110 10011110 010110.. ........ */
                    /* la.decode:2842 */
                    return LISA_XVFTINT_WU_S;
                case 0x17:
                    /* 01110110 10011110 010111.. ........ */
                    /* la.decode:2843 */
                    return LISA_XVFTINT_LU_D;
                case 0x18:
                    /* 01110110 10011110 011000.. ........ */
                    /* la.decode:2844 */
                    return LISA_XVFTINTRM_WU_S;
                case 0x19:
                    /* 01110110 10011110 011001.. ........ */
                    /* la.decode:2845 */
                    return LISA_XVFTINTRM_LU_D;
                case 0x1a:
                    /* 01110110 10011110 011010.. ........ */
                    /* la.decode:2846 */
                    return LISA_XVFTINTRP_WU_S;
                case 0x1b:
                    /* 01110110 10011110 011011.. ........ */
                    /* la.decode:2847 */
                    return LISA_XVFTINTRP_LU_D;
                case 0x1c:
                    /* 01110110 10011110 011100.. ........ */
                    /* la.decode:2848 */
                    return LISA_XVFTINTRZ_WU_S;
                case 0x1d:
                    /* 01110110 10011110 011101.. ........ */
                    /* la.decode:2849 */
                    return LISA_XVFTINTRZ_LU_D;
                case 0x1e:
                    /* 01110110 10011110 011110.. ........ */
                    /* la.decode:2850 */
                    return LISA_XVFTINTRNE_WU_S;
                case 0x1f:
                    /* 01110110 10011110 011111.. ........ */
                    /* la.decode:2851 */
                    return LISA_XVFTINTRNE_LU_D;
                }
                return LISA_INVALID;
            case 0x5:
                /* 01110110 10011110 1....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011110 100000.. ........ */
                    /* la.decode:2852 */
                    return LISA_XVFTINTL_L_S;
                case 0x1:
                    /* 01110110 10011110 100001.. ........ */
                    /* la.decode:2853 */
                    return LISA_XVFTINTH_L_S;
                case 0x2:
                    /* 01110110 10011110 100010.. ........ */
                    /* la.decode:2854 */
                    return LISA_XVFTINTRML_L_S;
                case 0x3:
                    /* 01110110 10011110 100011.. ........ */
                    /* la.decode:2855 */
                    return LISA_XVFTINTRMH_L_S;
                case 0x4:
                    /* 01110110 10011110 100100.. ........ */
                    /* la.decode:2856 */
                    return LISA_XVFTINTRPL_L_S;
                case 0x5:
                    /* 01110110 10011110 100101.. ........ */
                    /* la.decode:2857 */
                    return LISA_XVFTINTRPH_L_S;
                case 0x6:
                    /* 01110110 10011110 100110.. ........ */
                    /* la.decode:2858 */
                    return LISA_XVFTINTRZL_L_S;
                case 0x7:
                    /* 01110110 10011110 100111.. ........ */
                    /* la.decode:2859 */
                    return LISA_XVFTINTRZH_L_S;
                case 0x8:
                    /* 01110110 10011110 101000.. ........ */
                    /* la.decode:2860 */
                    return LISA_XVFTINTRNEL_L_S;
                case 0x9:
                    /* 01110110 10011110 101001.. ........ */
                    /* la.decode:2861 */
                    return LISA_XVFTINTRNEH_L_S;
                case 0xa:
                    /* 01110110 10011110 101010.. ........ */
                    /* la.decode:2862 */
                    return LISA_XVFTINTL_LU_S;
                case 0xb:
                    /* 01110110 10011110 101011.. ........ */
                    /* la.decode:2863 */
                    return LISA_XVFTINTH_LU_S;
                case 0xc:
                    /* 01110110 10011110 101100.. ........ */
                    /* la.decode:2864 */
                    return LISA_XVFTINTRML_LU_S;
                case 0xd:
                    /* 01110110 10011110 101101.. ........ */
                    /* la.decode:2865 */
                    return LISA_XVFTINTRMH_LU_S;
                case 0xe:
                    /* 01110110 10011110 101110.. ........ */
                    /* la.decode:2866 */
                    return LISA_XVFTINTRPL_LU_S;
                case 0xf:
                    /* 01110110 10011110 101111.. ........ */
                    /* la.decode:2867 */
                    return LISA_XVFTINTRPH_LU_S;
                case 0x10:
                    /* 01110110 10011110 110000.. ........ */
                    /* la.decode:2868 */
                    return LISA_XVFTINTRZL_LU_S;
                case 0x11:
                    /* 01110110 10011110 110001.. ........ */
                    /* la.decode:2869 */
                    return LISA_XVFTINTRZH_LU_S;
                case 0x12:
                    /* 01110110 10011110 110010.. ........ */
                    /* la.decode:2870 */
                    return LISA_XVFTINTRNEL_LU_S;
                case 0x13:
                    /* 01110110 10011110 110011.. ........ */
                    /* la.decode:2871 */
                    return LISA_XVFTINTRNEH_LU_S;
                case 0x18:
                    /* 01110110 10011110 111000.. ........ */
                    /* la.decode:2872 */
                    return LISA_XVEXTH_H_B;
                case 0x19:
                    /* 01110110 10011110 111001.. ........ */
                    /* la.decode:2873 */
                    return LISA_XVEXTH_W_H;
                case 0x1a:
                    /* 01110110 10011110 111010.. ........ */
                    /* la.decode:2874 */
                    return LISA_XVEXTH_D_W;
                case 0x1b:
                    /* 01110110 10011110 111011.. ........ */
                    /* la.decode:2875 */
                    return LISA_XVEXTH_Q_D;
                case 0x1c:
                    /* 01110110 10011110 111100.. ........ */
                    /* la.decode:2876 */
                    return LISA_XVEXTH_HU_BU;
                case 0x1d:
                    /* 01110110 10011110 111101.. ........ */
                    /* la.decode:2877 */
                    return LISA_XVEXTH_WU_HU;
                case 0x1e:
                    /* 01110110 10011110 111110.. ........ */
                    /* la.decode:2878 */
                    return LISA_XVEXTH_DU_WU;
                case 0x1f:
                    /* 01110110 10011110 111111.. ........ */
                    /* la.decode:2879 */
                    return LISA_XVEXTH_QU_DU;
                }
                return LISA_INVALID;
            case 0x6:
                /* 01110110 10011111 0....... ........ */
                switch ((insn >> 10) & 0x1f) {
                case 0x0:
                    /* 01110110 10011111 000000.. ........ */
                    /* la.decode:2880 */
                    return LISA_XVREPLGR2VR_B;
                case 0x1:
                    /* 01110110 10011111 000001.. ........ */
                    /* la.decode:2881 */
                    return LISA_XVREPLGR2VR_H;
                case 0x2:
                    /* 01110110 10011111 000010.. ........ */
                    /* la.decode:2882 */
                    return LISA_XVREPLGR2VR_W;
                case 0x3:
                    /* 01110110 10011111 000011.. ........ */
                    /* la.decode:2883 */
                    return LISA_XVREPLGR2VR_D;
                case 0x4:
                    /* 01110110 10011111 000100.. ........ */
                    /* la.decode:2884 */
                    return LISA_VEXT2XV_H_B;
                case 0x5:
                    /* 01110110 10011111 000101.. ........ */
                    /* la.decode:2885 */
                    return LISA_VEXT2XV_W_B;
                case 0x6:
                    /* 01110110 10011111 000110.. ........ */
                    /* la.decode:2886 */
                    return LISA_VEXT2XV_D_B;
                case 0x7:
                    /* 01110110 10011111 000111.. ........ */
                    /* la.decode:2887 */
                    return LISA_VEXT2XV_W_H;
                case 0x8:
                    /* 01110110 10011111 001000.. ........ */
                    /* la.decode:2888 */
                    return LISA_VEXT2XV_D_H;
                case 0x9:
                    /* 01110110 10011111 001001.. ........ */
                    /* la.decode:2889 */
                    return LISA_VEXT2XV_D_W;
                case 0xa:
                    /* 01110110 10011111 001010.. ........ */
                    /* la.decode:2890 */
                    return LISA_VEXT2XV_HU_BU;
                case 0xb:
                    /* 01110110 10011111 001011.. ........ */
                    /* la.decode:2891 */
                    return LISA_VEXT2XV_WU_BU;
                case 0xc:
                    /* 01110110 10011111 001100.. ........ */
                    /* la.decode:2892 */
                    return LISA_VEXT2XV_DU_BU;
                case 0xd:
                    /* 01110110 10011111 001101.. ........ */
                    /* la.decode:2893 */
                    return LISA_VEXT2XV_WU_HU;
                case 0xe:
                    /* 01110110 10011111 001110.. ........ */
                    /* la.decode:2894 */
                    return LISA_VEXT2XV_DU_HU;
                case 0xf:
                    /* 01110110 10011111 001111.. ........ */
                    /* la.decode:2895 */
                    return LISA_VEXT2XV_DU_WU;
                }
                return LISA_INVALID;
            case 0x7:
                /* 01110110 10011111 1....... ........ */
                /* la.decode:2896 */
                return LISA_XVHSELI_D;
            }
            return LISA_INVALID;
        case 0xa8:
            /* 01110110 101000.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110110 10100000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110110 10100000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110110 10100000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110110 10100000 001..... ........ */
                            /* la.decode:2897 */
                            return LISA_XVROTRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110110 10100000 01...... ........ */
                        /* la.decode:2898 */
                        return LISA_XVROTRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110110 10100000 1....... ........ */
                    /* la.decode:2899 */
                    return LISA_XVROTRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110110 10100001 ........ ........ */
                /* la.decode:2900 */
                return LISA_XVROTRI_D;
            }
            return LISA_INVALID;
        case 0xa9:
            /* 01110110 101001.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110110 10100100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110110 10100100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110110 10100100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110110 10100100 001..... ........ */
                            /* la.decode:2901 */
                            return LISA_XVSRLRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110110 10100100 01...... ........ */
                        /* la.decode:2902 */
                        return LISA_XVSRLRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110110 10100100 1....... ........ */
                    /* la.decode:2903 */
                    return LISA_XVSRLRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110110 10100101 ........ ........ */
                /* la.decode:2904 */
                return LISA_XVSRLRI_D;
            }
            return LISA_INVALID;
        case 0xaa:
            /* 01110110 101010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110110 10101000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110110 10101000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110110 10101000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110110 10101000 001..... ........ */
                            /* la.decode:2905 */
                            return LISA_XVSRARI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110110 10101000 01...... ........ */
                        /* la.decode:2906 */
                        return LISA_XVSRARI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110110 10101000 1....... ........ */
                    /* la.decode:2907 */
                    return LISA_XVSRARI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110110 10101001 ........ ........ */
                /* la.decode:2908 */
                return LISA_XVSRARI_D;
            }
            return LISA_INVALID;
        case 0xba:
            /* 01110110 111010.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x6:
                /* 01110110 11101011 0....... ........ */
                /* la.decode:2909 */
                return LISA_XVINSGR2VR_B;
            case 0x7:
                /* 01110110 11101011 1....... ........ */
                switch ((insn >> 14) & 0x1) {
                case 0x0:
                    /* 01110110 11101011 10...... ........ */
                    /* la.decode:2910 */
                    return LISA_XVINSGR2VR_H;
                case 0x1:
                    /* 01110110 11101011 11...... ........ */
                    switch ((insn >> 13) & 0x1) {
                    case 0x0:
                        /* 01110110 11101011 110..... ........ */
                        /* la.decode:2911 */
                        return LISA_XVINSGR2VR_W;
                    case 0x1:
                        /* 01110110 11101011 111..... ........ */
                        switch ((insn >> 12) & 0x1) {
                        case 0x0:
                            /* 01110110 11101011 1110.... ........ */
                            /* la.decode:2912 */
                            return LISA_XVINSGR2VR_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbb:
            /* 01110110 111011.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x6:
                /* 01110110 11101111 0....... ........ */
                /* la.decode:2913 */
                return LISA_XVPICKVE2GR_B;
            case 0x7:
                /* 01110110 11101111 1....... ........ */
                switch ((insn >> 14) & 0x1) {
                case 0x0:
                    /* 01110110 11101111 10...... ........ */
                    /* la.decode:2914 */
                    return LISA_XVPICKVE2GR_H;
                case 0x1:
                    /* 01110110 11101111 11...... ........ */
                    switch ((insn >> 13) & 0x1) {
                    case 0x0:
                        /* 01110110 11101111 110..... ........ */
                        /* la.decode:2915 */
                        return LISA_XVPICKVE2GR_W;
                    case 0x1:
                        /* 01110110 11101111 111..... ........ */
                        switch ((insn >> 12) & 0x1) {
                        case 0x0:
                            /* 01110110 11101111 1110.... ........ */
                            /* la.decode:2916 */
                            return LISA_XVPICKVE2GR_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbc:
            /* 01110110 111100.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x6:
                /* 01110110 11110011 0....... ........ */
                /* la.decode:2917 */
                return LISA_XVPICKVE2GR_BU;
            case 0x7:
                /* 01110110 11110011 1....... ........ */
                switch ((insn >> 14) & 0x1) {
                case 0x0:
                    /* 01110110 11110011 10...... ........ */
                    /* la.decode:2918 */
                    return LISA_XVPICKVE2GR_HU;
                case 0x1:
                    /* 01110110 11110011 11...... ........ */
                    switch ((insn >> 13) & 0x1) {
                    case 0x0:
                        /* 01110110 11110011 110..... ........ */
                        /* la.decode:2919 */
                        return LISA_XVPICKVE2GR_WU;
                    case 0x1:
                        /* 01110110 11110011 111..... ........ */
                        switch ((insn >> 12) & 0x1) {
                        case 0x0:
                            /* 01110110 11110011 1110.... ........ */
                            /* la.decode:2920 */
                            return LISA_XVPICKVE2GR_DU;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbd:
            /* 01110110 111101.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110110 11110111 10...... ........ */
                /* la.decode:2921 */
                return LISA_XVREPL128VEI_B;
            case 0xf:
                /* 01110110 11110111 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110110 11110111 110..... ........ */
                    /* la.decode:2922 */
                    return LISA_XVREPL128VEI_H;
                case 0x1:
                    /* 01110110 11110111 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110110 11110111 1110.... ........ */
                        /* la.decode:2923 */
                        return LISA_XVREPL128VEI_W;
                    case 0x1:
                        /* 01110110 11110111 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110110 11110111 11110... ........ */
                            /* la.decode:2924 */
                            return LISA_XVREPL128VEI_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbe:
            /* 01110110 111110.. ........ ........ */
            switch ((insn >> 14) & 0xf) {
            case 0xe:
                /* 01110110 11111011 10...... ........ */
                /* la.decode:2925 */
                return LISA_XVEXTRCOLI_B;
            case 0xf:
                /* 01110110 11111011 11...... ........ */
                switch ((insn >> 13) & 0x1) {
                case 0x0:
                    /* 01110110 11111011 110..... ........ */
                    /* la.decode:2926 */
                    return LISA_XVEXTRCOLI_H;
                case 0x1:
                    /* 01110110 11111011 111..... ........ */
                    switch ((insn >> 12) & 0x1) {
                    case 0x0:
                        /* 01110110 11111011 1110.... ........ */
                        /* la.decode:2927 */
                        return LISA_XVEXTRCOLI_W;
                    case 0x1:
                        /* 01110110 11111011 1111.... ........ */
                        switch ((insn >> 11) & 0x1) {
                        case 0x0:
                            /* 01110110 11111011 11110... ........ */
                            /* la.decode:2928 */
                            return LISA_XVEXTRCOLI_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xbf:
            /* 01110110 111111.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x6:
                /* 01110110 11111111 0....... ........ */
                /* la.decode:2929 */
                return LISA_XVINSVE0_B;
            case 0x7:
                /* 01110110 11111111 1....... ........ */
                switch ((insn >> 14) & 0x1) {
                case 0x0:
                    /* 01110110 11111111 10...... ........ */
                    /* la.decode:2930 */
                    return LISA_XVINSVE0_H;
                case 0x1:
                    /* 01110110 11111111 11...... ........ */
                    switch ((insn >> 13) & 0x1) {
                    case 0x0:
                        /* 01110110 11111111 110..... ........ */
                        /* la.decode:2931 */
                        return LISA_XVINSVE0_W;
                    case 0x1:
                        /* 01110110 11111111 111..... ........ */
                        switch ((insn >> 12) & 0x1) {
                        case 0x0:
                            /* 01110110 11111111 1110.... ........ */
                            /* la.decode:2932 */
                            return LISA_XVINSVE0_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xc0:
            /* 01110111 000000.. ........ ........ */
            switch ((insn >> 15) & 0x7) {
            case 0x6:
                /* 01110111 00000011 0....... ........ */
                /* la.decode:2933 */
                return LISA_XVPICKVE_B;
            case 0x7:
                /* 01110111 00000011 1....... ........ */
                switch ((insn >> 14) & 0x1) {
                case 0x0:
                    /* 01110111 00000011 10...... ........ */
                    /* la.decode:2934 */
                    return LISA_XVPICKVE_H;
                case 0x1:
                    /* 01110111 00000011 11...... ........ */
                    switch ((insn >> 13) & 0x1) {
                    case 0x0:
                        /* 01110111 00000011 110..... ........ */
                        /* la.decode:2935 */
                        return LISA_XVPICKVE_W;
                    case 0x1:
                        /* 01110111 00000011 111..... ........ */
                        switch ((insn >> 12) & 0x1) {
                        case 0x0:
                            /* 01110111 00000011 1110.... ........ */
                            /* la.decode:2936 */
                            return LISA_XVPICKVE_D;
                        }
                        return LISA_INVALID;
                    }
                    return LISA_INVALID;
                }
                return LISA_INVALID;
            }
            return LISA_INVALID;
        case 0xc1:
            /* 01110111 000001.. ........ ........ */
            switch ((insn >> 10) & 0xff) {
            case 0xc0:
                /* 01110111 00000111 000000.. ........ */
                /* la.decode:2937 */
                return LISA_XVREPLVE0_B;
            case 0xe0:
                /* 01110111 00000111 100000.. ........ */
                /* la.decode:2938 */
                return LISA_XVREPLVE0_H;
            case 0xf0:
                /* 01110111 00000111 110000.. ........ */
                /* la.decode:2939 */
                return LISA_XVREPLVE0_W;
            case 0xf8:
                /* 01110111 00000111 111000.. ........ */
                /* la.decode:2940 */
                return LISA_XVREPLVE0_D;
            case 0xfc:
                /* 01110111 00000111 111100.. ........ */
                /* la.decode:2941 */
                return LISA_XVREPLVE0_Q;
            }
            return LISA_INVALID;
        case 0xc2:
            /* 01110111 000010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00001000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00001000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00001000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00001000 001..... ........ */
                            /* la.decode:2942 */
                            return LISA_XVSLLWIL_H_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00001000 01...... ........ */
                        /* la.decode:2943 */
                        return LISA_XVSLLWIL_W_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00001000 1....... ........ */
                    /* la.decode:2944 */
                    return LISA_XVSLLWIL_D_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00001001 ........ ........ */
                if ((insn & 0x0000fc00) == 0x00000000) {
                    /* 01110111 00001001 000000.. ........ */
                    /* la.decode:2946 */
                    return LISA_XVEXTL_Q_D;
                }
                /* la.decode:2947 */
                return LISA_XVSLLWIL_Q_D;
            }
            return LISA_INVALID;
        case 0xc3:
            /* 01110111 000011.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00001100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00001100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00001100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00001100 001..... ........ */
                            /* la.decode:2949 */
                            return LISA_XVSLLWIL_HU_BU;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00001100 01...... ........ */
                        /* la.decode:2950 */
                        return LISA_XVSLLWIL_WU_HU;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00001100 1....... ........ */
                    /* la.decode:2951 */
                    return LISA_XVSLLWIL_DU_WU;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00001101 ........ ........ */
                if ((insn & 0x0000fc00) == 0x00000000) {
                    /* 01110111 00001101 000000.. ........ */
                    /* la.decode:2953 */
                    return LISA_XVEXTL_QU_DU;
                }
                /* la.decode:2954 */
                return LISA_XVSLLWIL_QU_DU;
            }
            return LISA_INVALID;
        case 0xc4:
            /* 01110111 000100.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00010000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00010000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00010000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00010000 001..... ........ */
                            /* la.decode:2956 */
                            return LISA_XVBITCLRI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00010000 01...... ........ */
                        /* la.decode:2957 */
                        return LISA_XVBITCLRI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00010000 1....... ........ */
                    /* la.decode:2958 */
                    return LISA_XVBITCLRI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00010001 ........ ........ */
                /* la.decode:2959 */
                return LISA_XVBITCLRI_D;
            }
            return LISA_INVALID;
        case 0xc5:
            /* 01110111 000101.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00010100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00010100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00010100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00010100 001..... ........ */
                            /* la.decode:2960 */
                            return LISA_XVBITSETI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00010100 01...... ........ */
                        /* la.decode:2961 */
                        return LISA_XVBITSETI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00010100 1....... ........ */
                    /* la.decode:2962 */
                    return LISA_XVBITSETI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00010101 ........ ........ */
                /* la.decode:2963 */
                return LISA_XVBITSETI_D;
            }
            return LISA_INVALID;
        case 0xc6:
            /* 01110111 000110.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00011000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00011000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00011000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00011000 001..... ........ */
                            /* la.decode:2964 */
                            return LISA_XVBITREVI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00011000 01...... ........ */
                        /* la.decode:2965 */
                        return LISA_XVBITREVI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00011000 1....... ........ */
                    /* la.decode:2966 */
                    return LISA_XVBITREVI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00011001 ........ ........ */
                /* la.decode:2967 */
                return LISA_XVBITREVI_D;
            }
            return LISA_INVALID;
        case 0xc7:
            /* 01110111 000111.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00011100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00011100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00011100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00011100 001..... ........ */
                            /* la.decode:2968 */
                            return LISA_XVBSTRC12I_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00011100 01...... ........ */
                        /* la.decode:2969 */
                        return LISA_XVBSTRC12I_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00011100 1....... ........ */
                    /* la.decode:2970 */
                    return LISA_XVBSTRC12I_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00011101 ........ ........ */
                /* la.decode:2971 */
                return LISA_XVBSTRC12I_D;
            }
            return LISA_INVALID;
        case 0xc8:
            /* 01110111 001000.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00100000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00100000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00100000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00100000 001..... ........ */
                            /* la.decode:2972 */
                            return LISA_XVBSTRC21I_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00100000 01...... ........ */
                        /* la.decode:2973 */
                        return LISA_XVBSTRC21I_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00100000 1....... ........ */
                    /* la.decode:2974 */
                    return LISA_XVBSTRC21I_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00100001 ........ ........ */
                /* la.decode:2975 */
                return LISA_XVBSTRC21I_D;
            }
            return LISA_INVALID;
        case 0xc9:
            /* 01110111 001001.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00100100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00100100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00100100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00100100 001..... ........ */
                            /* la.decode:2976 */
                            return LISA_XVSAT_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00100100 01...... ........ */
                        /* la.decode:2977 */
                        return LISA_XVSAT_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00100100 1....... ........ */
                    /* la.decode:2978 */
                    return LISA_XVSAT_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00100101 ........ ........ */
                /* la.decode:2979 */
                return LISA_XVSAT_D;
            }
            return LISA_INVALID;
        case 0xca:
            /* 01110111 001010.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00101000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00101000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00101000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00101000 001..... ........ */
                            /* la.decode:2980 */
                            return LISA_XVSAT_BU;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00101000 01...... ........ */
                        /* la.decode:2981 */
                        return LISA_XVSAT_HU;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00101000 1....... ........ */
                    /* la.decode:2982 */
                    return LISA_XVSAT_WU;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00101001 ........ ........ */
                /* la.decode:2983 */
                return LISA_XVSAT_DU;
            }
            return LISA_INVALID;
        case 0xcb:
            /* 01110111 001011.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00101100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00101100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00101100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00101100 001..... ........ */
                            /* la.decode:2984 */
                            return LISA_XVSLLI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00101100 01...... ........ */
                        /* la.decode:2985 */
                        return LISA_XVSLLI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00101100 1....... ........ */
                    /* la.decode:2986 */
                    return LISA_XVSLLI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00101101 ........ ........ */
                /* la.decode:2987 */
                return LISA_XVSLLI_D;
            }
            return LISA_INVALID;
        case 0xcc:
            /* 01110111 001100.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00110000 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00110000 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00110000 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00110000 001..... ........ */
                            /* la.decode:2988 */
                            return LISA_XVSRLI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00110000 01...... ........ */
                        /* la.decode:2989 */
                        return LISA_XVSRLI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00110000 1....... ........ */
                    /* la.decode:2990 */
                    return LISA_XVSRLI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00110001 ........ ........ */
                /* la.decode:2991 */
                return LISA_XVSRLI_D;
            }
            return LISA_INVALID;
        case 0xcd:
            /* 01110111 001101.. ........ ........ */
            switch ((insn >> 16) & 0x3) {
            case 0x0:
                /* 01110111 00110100 ........ ........ */
                switch ((insn >> 15) & 0x1) {
                case 0x0:
                    /* 01110111 00110100 0....... ........ */
                    switch ((insn >> 14) & 0x1) {
                    case 0x0:
                        /* 01110111 00110100 00...... ........ */
                        switch ((insn >> 13) & 0x1) {
                        case 0x1:
                            /* 01110111 00110100 001..... ........ */
                            /* la.decode:2992 */
                            return LISA_XVSRAI_B;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00110100 01...... ........ */
                        /* la.decode:2993 */
                        return LISA_XVSRAI_H;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00110100 1....... ........ */
                    /* la.decode:2994 */
                    return LISA_XVSRAI_W;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 00110101 ........ ........ */
                /* la.decode:2995 */
                return LISA_XVSRAI_D;
            }
            return LISA_INVALID;
        case 0xce:
            /* 01110111 001110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0011100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 00111000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 00111000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 00111000 01...... ........ */
                            /* la.decode:2996 */
                            return LISA_XVSRLRNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00111000 1....... ........ */
                        /* la.decode:2997 */
                        return LISA_XVSRLRNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00111001 ........ ........ */
                    /* la.decode:2998 */
                    return LISA_XVSRLRNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0011101. ........ ........ */
                /* la.decode:2999 */
                return LISA_XVSRLRNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xcf:
            /* 01110111 001111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0011110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 00111100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 00111100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 00111100 01...... ........ */
                            /* la.decode:3000 */
                            return LISA_XVSRARNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 00111100 1....... ........ */
                        /* la.decode:3001 */
                        return LISA_XVSRARNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 00111101 ........ ........ */
                    /* la.decode:3002 */
                    return LISA_XVSRARNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0011111. ........ ........ */
                /* la.decode:3003 */
                return LISA_XVSRARNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xd0:
            /* 01110111 010000.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0100000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01000000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01000000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01000000 01...... ........ */
                            /* la.decode:3004 */
                            return LISA_XVSRLNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01000000 1....... ........ */
                        /* la.decode:3005 */
                        return LISA_XVSRLNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01000001 ........ ........ */
                    /* la.decode:3006 */
                    return LISA_XVSRLNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0100001. ........ ........ */
                /* la.decode:3007 */
                return LISA_XVSRLNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd1:
            /* 01110111 010001.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0100010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01000100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01000100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01000100 01...... ........ */
                            /* la.decode:3008 */
                            return LISA_XVSRLRNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01000100 1....... ........ */
                        /* la.decode:3009 */
                        return LISA_XVSRLRNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01000101 ........ ........ */
                    /* la.decode:3010 */
                    return LISA_XVSRLRNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0100011. ........ ........ */
                /* la.decode:3011 */
                return LISA_XVSRLRNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd2:
            /* 01110111 010010.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0100100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01001000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01001000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01001000 01...... ........ */
                            /* la.decode:3012 */
                            return LISA_XVSSRLNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01001000 1....... ........ */
                        /* la.decode:3013 */
                        return LISA_XVSSRLNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01001001 ........ ........ */
                    /* la.decode:3014 */
                    return LISA_XVSSRLNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0100101. ........ ........ */
                /* la.decode:3015 */
                return LISA_XVSSRLNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd3:
            /* 01110111 010011.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0100110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01001100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01001100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01001100 01...... ........ */
                            /* la.decode:3016 */
                            return LISA_XVSSRLNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01001100 1....... ........ */
                        /* la.decode:3017 */
                        return LISA_XVSSRLNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01001101 ........ ........ */
                    /* la.decode:3018 */
                    return LISA_XVSSRLNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0100111. ........ ........ */
                /* la.decode:3019 */
                return LISA_XVSSRLNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xd4:
            /* 01110111 010100.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0101000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01010000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01010000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01010000 01...... ........ */
                            /* la.decode:3020 */
                            return LISA_XVSSRLRNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01010000 1....... ........ */
                        /* la.decode:3021 */
                        return LISA_XVSSRLRNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01010001 ........ ........ */
                    /* la.decode:3022 */
                    return LISA_XVSSRLRNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0101001. ........ ........ */
                /* la.decode:3023 */
                return LISA_XVSSRLRNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd5:
            /* 01110111 010101.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0101010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01010100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01010100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01010100 01...... ........ */
                            /* la.decode:3024 */
                            return LISA_XVSSRLRNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01010100 1....... ........ */
                        /* la.decode:3025 */
                        return LISA_XVSSRLRNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01010101 ........ ........ */
                    /* la.decode:3026 */
                    return LISA_XVSSRLRNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0101011. ........ ........ */
                /* la.decode:3027 */
                return LISA_XVSSRLRNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xd6:
            /* 01110111 010110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0101100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01011000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01011000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01011000 01...... ........ */
                            /* la.decode:3028 */
                            return LISA_XVSRANI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01011000 1....... ........ */
                        /* la.decode:3029 */
                        return LISA_XVSRANI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01011001 ........ ........ */
                    /* la.decode:3030 */
                    return LISA_XVSRANI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0101101. ........ ........ */
                /* la.decode:3031 */
                return LISA_XVSRANI_D_Q;
            }
            return LISA_INVALID;
        case 0xd7:
            /* 01110111 010111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0101110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01011100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01011100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01011100 01...... ........ */
                            /* la.decode:3032 */
                            return LISA_XVSRARNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01011100 1....... ........ */
                        /* la.decode:3033 */
                        return LISA_XVSRARNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01011101 ........ ........ */
                    /* la.decode:3034 */
                    return LISA_XVSRARNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0101111. ........ ........ */
                /* la.decode:3035 */
                return LISA_XVSRARNI_D_Q;
            }
            return LISA_INVALID;
        case 0xd8:
            /* 01110111 011000.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0110000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01100000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01100000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01100000 01...... ........ */
                            /* la.decode:3036 */
                            return LISA_XVSSRANI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01100000 1....... ........ */
                        /* la.decode:3037 */
                        return LISA_XVSSRANI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01100001 ........ ........ */
                    /* la.decode:3038 */
                    return LISA_XVSSRANI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0110001. ........ ........ */
                /* la.decode:3039 */
                return LISA_XVSSRANI_D_Q;
            }
            return LISA_INVALID;
        case 0xd9:
            /* 01110111 011001.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0110010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01100100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01100100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01100100 01...... ........ */
                            /* la.decode:3040 */
                            return LISA_XVSSRANI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01100100 1....... ........ */
                        /* la.decode:3041 */
                        return LISA_XVSSRANI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01100101 ........ ........ */
                    /* la.decode:3042 */
                    return LISA_XVSSRANI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0110011. ........ ........ */
                /* la.decode:3043 */
                return LISA_XVSSRANI_DU_Q;
            }
            return LISA_INVALID;
        case 0xda:
            /* 01110111 011010.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0110100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01101000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01101000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01101000 01...... ........ */
                            /* la.decode:3044 */
                            return LISA_XVSSRARNI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01101000 1....... ........ */
                        /* la.decode:3045 */
                        return LISA_XVSSRARNI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01101001 ........ ........ */
                    /* la.decode:3046 */
                    return LISA_XVSSRARNI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0110101. ........ ........ */
                /* la.decode:3047 */
                return LISA_XVSSRARNI_D_Q;
            }
            return LISA_INVALID;
        case 0xdb:
            /* 01110111 011011.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0110110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01101100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01101100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01101100 01...... ........ */
                            /* la.decode:3048 */
                            return LISA_XVSSRARNI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01101100 1....... ........ */
                        /* la.decode:3049 */
                        return LISA_XVSSRARNI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01101101 ........ ........ */
                    /* la.decode:3050 */
                    return LISA_XVSSRARNI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0110111. ........ ........ */
                /* la.decode:3051 */
                return LISA_XVSSRARNI_DU_Q;
            }
            return LISA_INVALID;
        case 0xdc:
            /* 01110111 011100.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0111000. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01110000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01110000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01110000 01...... ........ */
                            /* la.decode:3052 */
                            return LISA_XVSSRLRNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01110000 1....... ........ */
                        /* la.decode:3053 */
                        return LISA_XVSSRLRNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01110001 ........ ........ */
                    /* la.decode:3054 */
                    return LISA_XVSSRLRNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0111001. ........ ........ */
                /* la.decode:3055 */
                return LISA_XVSSRLRNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xdd:
            /* 01110111 011101.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0111010. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01110100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01110100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01110100 01...... ........ */
                            /* la.decode:3056 */
                            return LISA_XVSSRLRNENI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01110100 1....... ........ */
                        /* la.decode:3057 */
                        return LISA_XVSSRLRNENI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01110101 ........ ........ */
                    /* la.decode:3058 */
                    return LISA_XVSSRLRNENI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0111011. ........ ........ */
                /* la.decode:3059 */
                return LISA_XVSSRLRNENI_DU_Q;
            }
            return LISA_INVALID;
        case 0xde:
            /* 01110111 011110.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0111100. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01111000 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01111000 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01111000 01...... ........ */
                            /* la.decode:3060 */
                            return LISA_XVSSRARNENI_B_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01111000 1....... ........ */
                        /* la.decode:3061 */
                        return LISA_XVSSRARNENI_H_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01111001 ........ ........ */
                    /* la.decode:3062 */
                    return LISA_XVSSRARNENI_W_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0111101. ........ ........ */
                /* la.decode:3063 */
                return LISA_XVSSRARNENI_D_Q;
            }
            return LISA_INVALID;
        case 0xdf:
            /* 01110111 011111.. ........ ........ */
            switch ((insn >> 17) & 0x1) {
            case 0x0:
                /* 01110111 0111110. ........ ........ */
                switch ((insn >> 16) & 0x1) {
                case 0x0:
                    /* 01110111 01111100 ........ ........ */
                    switch ((insn >> 15) & 0x1) {
                    case 0x0:
                        /* 01110111 01111100 0....... ........ */
                        switch ((insn >> 14) & 0x1) {
                        case 0x1:
                            /* 01110111 01111100 01...... ........ */
                            /* la.decode:3064 */
                            return LISA_XVSSRARNENI_BU_H;
                        }
                        return LISA_INVALID;
                    case 0x1:
                        /* 01110111 01111100 1....... ........ */
                        /* la.decode:3065 */
                        return LISA_XVSSRARNENI_HU_W;
                    }
                    return LISA_INVALID;
                case 0x1:
                    /* 01110111 01111101 ........ ........ */
                    /* la.decode:3066 */
                    return LISA_XVSSRARNENI_WU_D;
                }
                return LISA_INVALID;
            case 0x1:
                /* 01110111 0111111. ........ ........ */
                /* la.decode:3067 */
                return LISA_XVSSRARNENI_DU_Q;
            }
            return LISA_INVALID;
        case 0xe0:
            /* 01110111 100000.. ........ ........ */
            /* la.decode:3068 */
            return LISA_XVEXTRINS_D;
        case 0xe1:
            /* 01110111 100001.. ........ ........ */
            /* la.decode:3069 */
            return LISA_XVEXTRINS_W;
        case 0xe2:
            /* 01110111 100010.. ........ ........ */
            /* la.decode:3070 */
            return LISA_XVEXTRINS_H;
        case 0xe3:
            /* 01110111 100011.. ........ ........ */
            /* la.decode:3071 */
            return LISA_XVEXTRINS_B;
        case 0xe4:
            /* 01110111 100100.. ........ ........ */
            /* la.decode:3072 */
            return LISA_XVSHUF4I_B;
        case 0xe5:
            /* 01110111 100101.. ........ ........ */
            /* la.decode:3073 */
            return LISA_XVSHUF4I_H;
        case 0xe6:
            /* 01110111 100110.. ........ ........ */
            /* la.decode:3074 */
            return LISA_XVSHUF4I_W;
        case 0xe7:
            /* 01110111 100111.. ........ ........ */
            /* la.decode:3075 */
            return LISA_XVSHUF4I_D;
        case 0xe8:
            /* 01110111 101000.. ........ ........ */
            /* la.decode:3076 */
            return LISA_XVSHUFI1_B;
        case 0xe9:
            /* 01110111 101001.. ........ ........ */
            /* la.decode:3077 */
            return LISA_XVSHUFI2_B;
        case 0xea:
            /* 01110111 101010.. ........ ........ */
            /* la.decode:3078 */
            return LISA_XVSHUFI3_B;
        case 0xeb:
            /* 01110111 101011.. ........ ........ */
            /* la.decode:3079 */
            return LISA_XVSHUFI4_B;
        case 0xec:
            /* 01110111 101100.. ........ ........ */
            /* la.decode:3080 */
            return LISA_XVSHUFI1_H;
        case 0xed:
            /* 01110111 101101.. ........ ........ */
            /* la.decode:3081 */
            return LISA_XVSHUFI2_H;
        case 0xee:
            /* 01110111 101110.. ........ ........ */
            /* la.decode:3082 */
            return LISA_XVSELI_H;
        case 0xef:
            /* 01110111 101111.. ........ ........ */
            /* la.decode:3083 */
            return LISA_XVSELI_W;
        case 0xf0:
            /* 01110111 110000.. ........ ........ */
            /* la.decode:3084 */
            return LISA_XVSELI_D;
        case 0xf1:
            /* 01110111 110001.. ........ ........ */
            /* la.decode:3085 */
            return LISA_XVBITSELI_B;
        case 0xf2:
            /* 01110111 110010.. ........ ........ */
            /* la.decode:3086 */
            return LISA_XVBITMVZI_B;
        case 0xf3:
            /* 01110111 110011.. ........ ........ */
            /* la.decode:3087 */
            return LISA_XVBITMVNZI_B;
        case 0xf4:
            /* 01110111 110100.. ........ ........ */
            /* la.decode:3088 */
            return LISA_XVANDI_B;
        case 0xf5:
            /* 01110111 110101.. ........ ........ */
            /* la.decode:3089 */
            return LISA_XVORI_B;
        case 0xf6:
            /* 01110111 110110.. ........ ........ */
            /* la.decode:3090 */
            return LISA_XVXORI_B;
        case 0xf7:
            /* 01110111 110111.. ........ ........ */
            /* la.decode:3091 */
            return LISA_XVNORI_B;
        case 0xf8:
            /* 01110111 111000.. ........ ........ */
            /* la.decode:3092 */
            return LISA_XVLDI;
        case 0xf9:
            /* 01110111 111001.. ........ ........ */
            /* la.decode:3093 */
            return LISA_XVPERMI_W;
        case 0xfa:
            /* 01110111 111010.. ........ ........ */
            /* la.decode:3094 */
            return LISA_XVPERMI_D;
        case 0xfb:
            /* 01110111 111011.. ........ ........ */
            /* la.decode:3095 */
            return LISA_XVPERMI_Q;
        }
        return LISA_INVALID;
    }
    return LISA_INVALID;
}
