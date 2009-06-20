/* File automatically generated by genmdesc, don't change */

const char amd64_desc [] = {
	"\x0\x0\x0\x0\x0"	/* null entry */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* nop */
	"\x0" "\x0" "\x0" "\x1" "\x0" ""	/* hard_nop */
	"\x0" "\x0" "\x0" "\x2" "\x0" ""	/* relaxed_nop */
	"\x0" "ii\x3" "\x0" ""	/* compare */
	"\x0" "i\x0" "\xd" "\x0" ""	/* compare_imm */
	"\x0" "ff\xd" "a"	/* fcompare */
	"\x0" "ii\x3" "\x0" ""	/* lcompare */
	"\x0" "ii\x3" "\x0" ""	/* icompare */
	"\x0" "i\x0" "\x8" "\x0" ""	/* icompare_imm */
	"\x0" "i\x0" "\xd" "\x0" ""	/* lcompare_imm */
	"\x0" "b\x0" "\xb" "\x0" ""	/* oparglist */
	"iii\x5" "\x0" ""	/* setlret */
	"ii\x0" "T\x0" ""	/* localloc */
	"i\x0" "\x0" "T\x0" ""	/* localloc_imm */
	"\x0" "b\x0" "\x5" "\x0" ""	/* checkthis */
	"\x0" "\x0" "\x0" "\x20" "c"	/* voidcall */
	"\x0" "i\x0" "\x20" "c"	/* voidcall_reg */
	"\x0" "b\x0" "\x20" "c"	/* voidcall_membase */
	"a\x0" "\x0" "\x20" "c"	/* call */
	"ai\x0" "\x20" "c"	/* call_reg */
	"ab\x0" "\x20" "c"	/* call_membase */
	"f\x0" "\x0" "\x40" "c"	/* fcall */
	"fi\x0" "\x40" "c"	/* fcall_reg */
	"fb\x0" "\x40" "c"	/* fcall_membase */
	"a\x0" "\x0" "\x40" "c"	/* lcall */
	"ai\x0" "\x40" "c"	/* lcall_reg */
	"ab\x0" "\x40" "c"	/* lcall_membase */
	"\x0" "\x0" "\x0" "\x40" "c"	/* vcall */
	"\x0" "i\x0" "\x40" "c"	/* vcall_reg */
	"\x0" "b\x0" "\x40" "c"	/* vcall_membase */
	"\x0" "\x0" "\x0" "\x40" "c"	/* vcall2 */
	"\x0" "i\x0" "\x40" "c"	/* vcall2_reg */
	"\x0" "b\x0" "\x40" "c"	/* vcall2_membase */
	"i\x0" "\x0" "\xa" "\x0" ""	/* iconst */
	"i\x0" "\x0" "\xa" "\x0" ""	/* i8const */
	"f\x0" "\x0" "\xe" "\x0" ""	/* r4const */
	"f\x0" "\x0" "\x9" "\x0" ""	/* r8const */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* label */
	"\x0" "i\x0" "\x12" "\x0" ""	/* throw */
	"\x0" "i\x0" "\x12" "\x0" ""	/* rethrow */
	"bi\x0" "\x9" "\x0" ""	/* store_membase_reg */
	"bc\x0" "\x9" "\x0" ""	/* storei1_membase_reg */
	"bi\x0" "\x9" "\x0" ""	/* storei2_membase_reg */
	"bi\x0" "\x9" "\x0" ""	/* storei4_membase_reg */
	"bi\x0" "\x9" "\x0" ""	/* storei8_membase_reg */
	"bf\x0" "\xf" "\x0" ""	/* storer4_membase_reg */
	"bf\x0" "\xa" "\x0" ""	/* storer8_membase_reg */
	"b\x0" "\x0" "\xf" "\x0" ""	/* store_membase_imm */
	"b\x0" "\x0" "\xb" "\x0" ""	/* storei1_membase_imm */
	"b\x0" "\x0" "\xd" "\x0" ""	/* storei2_membase_imm */
	"b\x0" "\x0" "\xd" "\x0" ""	/* storei4_membase_imm */
	"b\x0" "\x0" "\x12" "\x0" ""	/* storei8_membase_imm */
	"ib\x0" "\x8" "\x0" ""	/* load_membase */
	"cb\x0" "\x9" "\x0" ""	/* loadi1_membase */
	"cb\x0" "\x9" "\x0" ""	/* loadu1_membase */
	"ib\x0" "\x9" "\x0" ""	/* loadi2_membase */
	"ib\x0" "\x9" "\x0" ""	/* loadu2_membase */
	"ib\x0" "\x9" "\x0" ""	/* loadi4_membase */
	"ib\x0" "\x9" "\x0" ""	/* loadu4_membase */
	"ib\x0" "\x12" "\x0" ""	/* loadi8_membase */
	"fb\x0" "\x10" "\x0" ""	/* loadr4_membase */
	"fb\x0" "\x10" "\x0" ""	/* loadr8_membase */
	"\x0" "b\x0" "\x9" "\x0" ""	/* loadr8_spill_membase */
	"i\x0" "\x0" "\x10" "\x0" ""	/* load_mem */
	"i\x0" "\x0" "\x10" "\x0" ""	/* loadu1_mem */
	"i\x0" "\x0" "\x10" "\x0" ""	/* loadu2_mem */
	"i\x0" "\x0" "\x10" "\x0" ""	/* loadi4_mem */
	"i\x0" "\x0" "\xa" "\x0" ""	/* loadu4_mem */
	"i\x0" "\x0" "\x10" "\x0" ""	/* loadi8_mem */
	"ii\x0" "\x3" "\x0" ""	/* move */
	"ff\x0" "\x8" "\x0" ""	/* fmove */
	"ii\x0" "\x8" "1"	/* add_imm */
	"ii\x0" "\x8" "1"	/* sub_imm */
	"ii\x0" "\xb" "\x0" ""	/* mul_imm */
	"aii\x10" "d"	/* div_imm */
	"aii\x10" "d"	/* div_un_imm */
	"dii\x10" "a"	/* rem_imm */
	"dii\x10" "a"	/* rem_un_imm */
	"ii\x0" "\x8" "1"	/* and_imm */
	"ii\x0" "\x8" "1"	/* or_imm */
	"ii\x0" "\x8" "1"	/* xor_imm */
	"ii\x0" "\x8" "1"	/* shl_imm */
	"ii\x0" "\x8" "1"	/* shr_imm */
	"ii\x0" "\x8" "1"	/* shr_un_imm */
	"\x0" "\x0" "\x0" "\x6" "\x0" ""	/* br */
	"\x0" "\x0" "\x0" "x\x0" ""	/* jmp */
	"\x0" "\x0" "\x0" "xc"	/* tailcall */
	"\x0" "\x0" "\x0" "\x2" "\x0" ""	/* break */
	"c\x0" "\x0" "\x8" "\x0" ""	/* ceq */
	"c\x0" "\x0" "\x8" "\x0" ""	/* cgt */
	"c\x0" "\x0" "\x8" "\x0" ""	/* cgt.un */
	"c\x0" "\x0" "\x8" "\x0" ""	/* clt */
	"c\x0" "\x0" "\x8" "\x0" ""	/* clt.un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_eq */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ge */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_gt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_le */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_lt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ne_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ge_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_gt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_le_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_lt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ov */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_no */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_c */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_nc */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ieq */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ige */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_igt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ile */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ilt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ine_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ige_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_igt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ile_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ilt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_iov */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ino */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_ic */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* cond_exc_inc */
	"iii\x3" "1"	/* long_add */
	"iii\x3" "1"	/* long_sub */
	"iii\x4" "1"	/* long_mul */
	"aai\x10" "d"	/* long_div */
	"aai\x10" "d"	/* long_div_un */
	"dai\x10" "a"	/* long_rem */
	"dai\x10" "a"	/* long_rem_un */
	"iii\x3" "1"	/* long_and */
	"iii\x3" "1"	/* long_or */
	"iii\x3" "1"	/* long_xor */
	"iis\x3" "1"	/* long_shl */
	"iis\x3" "1"	/* long_shr */
	"iis\x3" "1"	/* long_shr_un */
	"ii\x0" "\x3" "1"	/* long_neg */
	"ii\x0" "\x3" "1"	/* long_not */
	"ii\x0" "\x4" "\x0" ""	/* long_conv_to_i1 */
	"ii\x0" "\x4" "\x0" ""	/* long_conv_to_i2 */
	"ii\x0" "\x3" "\x0" ""	/* long_conv_to_i4 */
	"ii\x0" "\x3" "\x0" ""	/* long_conv_to_i8 */
	"fi\x0" "\x9" "\x0" ""	/* long_conv_to_r4 */
	"fi\x0" "\x9" "\x0" ""	/* long_conv_to_r8 */
	"ii\x0" "\x3" "\x0" ""	/* long_conv_to_u4 */
	"ii\x0" "\x3" "\x0" ""	/* long_conv_to_u8 */
	"ii\x0" "\x4" "\x0" ""	/* long_conv_to_u2 */
	"ii\x0" "\x4" "\x0" ""	/* long_conv_to_u1 */
	"iii\x28" "\x0" ""	/* long_conv_to_ovf_i */
	"iii\x10" "1"	/* long_mul_ovf */
	"iii\x16" "\x0" ""	/* long_mul_ovf_un */
	"ii\x0" "\x10" "\x0" ""	/* long_conv_to_ovf_i4_un */
	"ii\x0" "\xf" "\x0" ""	/* long_conv_to_ovf_u4 */
	"c\x0" "\x0" "\x40" "\x0" ""	/* long_ceq */
	"c\x0" "\x0" "\x40" "\x0" ""	/* long_cgt */
	"c\x0" "\x0" "\x40" "\x0" ""	/* long_cgt_un */
	"c\x0" "\x0" "\x40" "\x0" ""	/* long_clt */
	"c\x0" "\x0" "\x40" "\x0" ""	/* long_clt_un */
	"fi\x0" "\x40" "\x0" ""	/* long_conv_to_r_un */
	"ii\x0" "\xc" "1"	/* long_add_imm */
	"ii\x0" "\xc" "1"	/* long_sub_imm */
	"ii\x0" "\xc" "1"	/* long_mul_imm */
	"ii\x0" "\xc" "1"	/* long_and_imm */
	"ii\x0" "\xc" "1"	/* long_or_imm */
	"ii\x0" "\xc" "1"	/* long_xor_imm */
	"ii\x0" "\xb" "1"	/* long_shl_imm */
	"ii\x0" "\xb" "1"	/* long_shr_imm */
	"ii\x0" "\xb" "1"	/* long_shr_un_imm */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_beq */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_bge */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_bgt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_ble */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_blt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_bne_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_bge_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_bgt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_ble_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* long_blt_un */
	"iii\x4" "1"	/* int_add */
	"iii\x4" "1"	/* int_sub */
	"iii\x4" "1"	/* int_mul */
	"aai\x20" "d"	/* int_div */
	"aai\x20" "d"	/* int_div_un */
	"dai\x20" "a"	/* int_rem */
	"dai\x20" "a"	/* int_rem_un */
	"iii\x4" "1"	/* int_and */
	"iii\x4" "1"	/* int_or */
	"iii\x4" "1"	/* int_xor */
	"iis\x4" "1"	/* int_shl */
	"iis\x4" "1"	/* int_shr */
	"iis\x4" "1"	/* int_shr_un */
	"ii\x0" "\x4" "1"	/* int_neg */
	"ii\x0" "\x4" "1"	/* int_not */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_i1 */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_i2 */
	"ii\x0" "\x3" "\x0" ""	/* int_conv_to_i4 */
	"ii\x0" "\x3" "\x0" ""	/* int_conv_to_i8 */
	"fi\x0" "\x9" "\x0" ""	/* int_conv_to_r4 */
	"fi\x0" "\x9" "\x0" ""	/* int_conv_to_r8 */
	"ii\x0" "\x3" "\x0" ""	/* int_conv_to_u4 */
	"ii\x0" "\x3" "\x0" ""	/* int_conv_to_u8 */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_u */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_u2 */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_u1 */
	"ii\x0" "\x4" "\x0" ""	/* int_conv_to_i */
	"iii\x20" "1"	/* int_mul_ovf */
	"iii\x20" "1"	/* int_mul_ovf_un */
	"iii\x4" "1"	/* int_adc */
	"ii\x0" "\x8" "1"	/* int_adc_imm */
	"iii\x4" "1"	/* int_sbb */
	"ii\x0" "\x8" "1"	/* int_sbb_imm */
	"iii\x10" "1"	/* int_addcc */
	"iii\x10" "1"	/* int_subcc */
	"ii\x0" "\x8" "1"	/* int_add_imm */
	"ii\x0" "\x8" "1"	/* int_sub_imm */
	"ii\x0" "\x20" "1"	/* int_mul_imm */
	"ai\x0" "\x20" "d"	/* int_div_imm */
	"ai\x0" "\x20" "d"	/* int_div_un_imm */
	"aa\x0" "\x20" "d"	/* int_rem_imm */
	"di\x0" "\x20" "a"	/* int_rem_un_imm */
	"ii\x0" "\x8" "1"	/* int_and_imm */
	"ii\x0" "\x8" "1"	/* int_or_imm */
	"ii\x0" "\x8" "1"	/* int_xor_imm */
	"ii\x0" "\x8" "1"	/* int_shl_imm */
	"ii\x0" "\x8" "1"	/* int_shr_imm */
	"ii\x0" "\x8" "1"	/* int_shr_un_imm */
	"c\x0" "\x0" "\x8" "\x0" ""	/* int_ceq */
	"c\x0" "\x0" "\x8" "\x0" ""	/* int_cgt */
	"c\x0" "\x0" "\x8" "\x0" ""	/* int_cgt_un */
	"c\x0" "\x0" "\x8" "\x0" ""	/* int_clt */
	"c\x0" "\x0" "\x8" "\x0" ""	/* int_clt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_beq */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_bge */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_bgt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_ble */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_blt */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_bne_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_bge_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_bgt_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_ble_un */
	"\x0" "\x0" "\x0" "\x8" "\x0" ""	/* int_blt_un */
	"\x0" "\x0" "\x0" "\xd" "\x0" ""	/* float_beq */
	"\x0" "\x0" "\x0" "\x20" "\x0" ""	/* float_bge */
	"\x0" "\x0" "\x0" "\xd" "\x0" ""	/* float_bgt */
	"\x0" "\x0" "\x0" "\x20" "\x0" ""	/* float_ble */
	"\x0" "\x0" "\x0" "\xd" "\x0" ""	/* float_blt */
	"\x0" "\x0" "\x0" "\x12" "\x0" ""	/* float_bne_un */
	"\x0" "\x0" "\x0" "\xd" "\x0" ""	/* float_bge_un */
	"\x0" "\x0" "\x0" "\x1e" "\x0" ""	/* float_bgt_un */
	"\x0" "\x0" "\x0" "\xd" "\x0" ""	/* float_ble_un */
	"\x0" "\x0" "\x0" "\x1e" "\x0" ""	/* float_blt_un */
	"fff\x5" "1"	/* float_add */
	"fff\x5" "1"	/* float_sub */
	"fff\x5" "1"	/* float_mul */
	"fff\x5" "1"	/* float_div */
	"fff\x5" "1"	/* float_div_un */
	"fff\x13" "1"	/* float_rem */
	"fff\x13" "1"	/* float_rem_un */
	"ff\x0" "\x17" "1"	/* float_neg */
	"ff\x0" "\x3" "1"	/* float_not */
	"if\x0" "1\x0" ""	/* float_conv_to_i1 */
	"if\x0" "1\x0" ""	/* float_conv_to_i2 */
	"if\x0" "1\x0" ""	/* float_conv_to_i4 */
	"if\x0" "1\x0" ""	/* float_conv_to_i8 */
	"ff\x0" "\x0" "\x0" ""	/* float_conv_to_r4 */
	"if\x0" "1\x0" ""	/* float_conv_to_u4 */
	"if\x0" "1\x0" ""	/* float_conv_to_u8 */
	"if\x0" "1\x0" ""	/* float_conv_to_u2 */
	"if\x0" "1\x0" ""	/* float_conv_to_u1 */
	"if\x0" "1\x0" ""	/* float_conv_to_i */
	"af\x0" "\x28" "\x0" ""	/* float_conv_to_ovf_i */
	"af\x0" "\x28" "\x0" ""	/* float_conv_to_ovd_u */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* float_mul_ovf */
	"iff\x23" "\x0" ""	/* float_ceq */
	"iff\x23" "\x0" ""	/* float_cgt */
	"iff0\x0" ""	/* float_cgt_un */
	"iff\x23" "\x0" ""	/* float_clt */
	"iff\x2a" "\x0" ""	/* float_clt_un */
	"ifb\x23" "\x0" ""	/* float_ceq_membase */
	"ifb\x23" "\x0" ""	/* float_cgt_membase */
	"ifb0\x0" ""	/* float_cgt_un_membase */
	"ifb\x23" "\x0" ""	/* float_clt_membase */
	"ifb\x2a" "\x0" ""	/* float_clt_un_membase */
	"if\x0" "\x2e" "\x0" ""	/* float_conv_to_u */
	"ff\x0" "\x2b" "\x0" ""	/* ckfinite */
	"i\x0" "\x0" "\x12" "\x0" ""	/* jump_table */
	"i\x0" "\x0" "\xa" "\x0" ""	/* aot_const */
	"\x0" "\x0" "\x0" "\xe" "\x0" ""	/* call_handler */
	"\x0" "\x0" "\x0" "\x9" "\x0" ""	/* start_handler */
	"\x0" "a\x0" "\x9" "\x0" ""	/* endfilter */
	"\x0" "\x0" "\x0" "\x9" "\x0" ""	/* endfinally */
	"iai\x3" "\x0" ""	/* bigmul */
	"iai\x3" "\x0" ""	/* bigmul_un */
	"iii\x10" "1"	/* int_min_un */
	"iii\x10" "1"	/* int_max_un */
	"iii\x10" "1"	/* long_min_un */
	"iii\x10" "1"	/* long_max_un */
	"iii\x10" "1"	/* int_min */
	"iii\x10" "1"	/* int_max */
	"iii\x10" "1"	/* long_min */
	"iii\x10" "1"	/* long_max */
	"iii\x3" "1"	/* adc */
	"ii\x0" "\x8" "1"	/* adc_imm */
	"iii\x3" "1"	/* sbb */
	"ii\x0" "\x8" "1"	/* sbb_imm */
	"iii\x3" "1"	/* addcc */
	"iii\x3" "1"	/* subcc */
	"\x0" "i\x0" "\x3" "\x0" ""	/* br_reg */
	"ii\x0" "\x4" "\x0" ""	/* sext_i1 */
	"ii\x0" "\x4" "\x0" ""	/* sext_i2 */
	"ii\x0" "\x8" "\x0" ""	/* sext_i4 */
	"ii\x0" "\x4" "\x0" ""	/* zext_i4 */
	"ff\x0" "\x20" "\x0" ""	/* sin */
	"ff\x0" "\x20" "\x0" ""	/* cos */
	"ff\x0" "\x20" "1"	/* abs */
	"ff\x0" "\x3b" "\x0" ""	/* tan */
	"ff\x0" "\x9" "\x0" ""	/* atan */
	"ff\x0" "\x20" "\x0" ""	/* sqrt */
	"i\x0" "\x0" "\x10" "\x0" ""	/* tls_get */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* dummy_use */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* dummy_store */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* not_reached */
	"\x0" "i\x0" "\x0" "\x0" ""	/* not_null */
	"ibi\x20" "\x0" ""	/* atomic_add_i4 */
	"ibi\x20" "\x0" ""	/* atomic_add_new_i4 */
	"abi\x20" "\x0" ""	/* atomic_exchange_i4 */
	"ibi\x20" "\x0" ""	/* atomic_add_i8 */
	"ibi\x20" "\x0" ""	/* atomic_add_new_i8 */
	"abi\x20" "\x0" ""	/* atomic_exchange_i8 */
	"\x0" "\x0" "\x0" "\x10" "\x0" ""	/* memory_barrier */
	"abi\x20" "\x0" ""	/* atomic_cas_imm_i4 */
	"abi\x20" "\x0" ""	/* atomic_cas_imm_i8 */
	"iii\x10" "1"	/* cmov_ieq */
	"iii\x10" "1"	/* cmov_ige */
	"iii\x10" "1"	/* cmov_igt */
	"iii\x10" "1"	/* cmov_ile */
	"iii\x10" "1"	/* cmov_ilt */
	"iii\x10" "1"	/* cmov_ine_un */
	"iii\x10" "1"	/* cmov_ige_un */
	"iii\x10" "1"	/* cmov_igt_un */
	"iii\x10" "1"	/* cmov_ile_un */
	"iii\x10" "1"	/* cmov_ilt_un */
	"iii\x10" "1"	/* cmov_leq */
	"iii\x10" "1"	/* cmov_lge */
	"iii\x10" "1"	/* cmov_lgt */
	"iii\x10" "1"	/* cmov_lle */
	"iii\x10" "1"	/* cmov_llt */
	"iii\x10" "1"	/* cmov_lne_un */
	"iii\x10" "1"	/* cmov_lge_un */
	"iii\x10" "1"	/* cmov_lgt_un */
	"iii\x10" "1"	/* cmov_lle_un */
	"iii\x10" "1"	/* cmov_llt_un */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* liverange_start */
	"\x0" "\x0" "\x0" "\x0" "\x0" ""	/* liverange_end */
	"\x0" "i\x0" "\x5" "\x0" ""	/* x86_test_null */
	"\x0" "bi\x9" "\x0" ""	/* x86_compare_membase_reg */
	"\x0" "b\x0" "\xd" "\x0" ""	/* x86_compare_membase_imm */
	"\x0" "b\x0" "\x9" "\x0" ""	/* x86_compare_membase8_imm */
	"\x0" "ib\x8" "\x0" ""	/* x86_compare_reg_membase */
	"ii\x0" "\x3" "1"	/* x86_inc_reg */
	"\x0" "b\x0" "\x8" "\x0" ""	/* x86_inc_membase */
	"ii\x0" "\x3" "1"	/* x86_dec_reg */
	"\x0" "b\x0" "\x8" "\x0" ""	/* x86_dec_membase */
	"\x0" "b\x0" "\xd" "\x0" ""	/* x86_add_membase_imm */
	"\x0" "b\x0" "\xd" "\x0" ""	/* x86_sub_membase_imm */
	"\x0" "b\x0" "\xc" "\x0" ""	/* x86_and_membase_imm */
	"\x0" "b\x0" "\xc" "\x0" ""	/* x86_or_membase_imm */
	"\x0" "b\x0" "\xc" "\x0" ""	/* x86_xor_membase_imm */
	"\x0" "bi\xc" "\x0" ""	/* x86_add_membase_reg */
	"\x0" "bi\xc" "\x0" ""	/* x86_sub_membase_reg */
	"\x0" "bi\xc" "\x0" ""	/* x86_and_membase_reg */
	"\x0" "bi\xc" "\x0" ""	/* x86_or_membase_reg */
	"\x0" "bi\xc" "\x0" ""	/* x86_xor_membase_reg */
	"\x0" "bi\xe" "\x0" ""	/* x86_mul_membase_reg */
	"iib\xd" "1"	/* x86_add_reg_membase */
	"iib\xd" "1"	/* x86_sub_reg_membase */
	"iib\xd" "1"	/* x86_mul_reg_membase */
	"iib\xd" "1"	/* x86_and_reg_membase */
	"iib\xd" "1"	/* x86_or_reg_membase */
	"iib\xd" "1"	/* x86_xor_reg_membase */
	"\x0" "b\x0" "\x8" "\x0" ""	/* x86_push_membase */
	"\x0" "\x0" "\x0" "\x6" "\x0" ""	/* x86_push_imm */
	"\x0" "i\x0" "\x3" "\x0" ""	/* x86_push */
	"\x0" "b\x0" "\x28" "\x0" ""	/* x86_push_obj */
	"iii\x8" "\x0" ""	/* x86_lea */
	"ii\x0" "\xb" "\x0" ""	/* x86_lea_membase */
	"\x0" "ii\x2" "x"	/* x86_xchg */
	"\x0" "f\x0" "\x3" "\x0" ""	/* x86_fpop */
	"\x0" "b\x0" "\x9" "\x0" ""	/* x86_seteq_membase */
	"\x0" "i\x0" "\x5" "\x0" ""	/* amd64_test_null */
	"ff\x0" "\xe" "m"	/* amd64_set_xmmreg_r4 */
	"ff\x0" "\xe" "m"	/* amd64_set_xmmreg_r8 */
	"\x0" "bi\x8" "\x0" ""	/* amd64_icompare_membase_reg */
	"\x0" "b\x0" "\xd" "\x0" ""	/* amd64_icompare_membase_imm */
	"\x0" "ib\x8" "\x0" ""	/* amd64_icompare_reg_membase */
	"\x0" "bi\x9" "\x0" ""	/* amd64_compare_membase_reg */
	"\x0" "b\x0" "\xe" "\x0" ""	/* amd64_compare_membase_imm */
	"\x0" "ib\x9" "\x0" ""	/* amd64_compare_reg_membase */
	"\x0" "bi\xd" "\x0" ""	/* amd64_add_membase_reg */
	"\x0" "bi\xd" "\x0" ""	/* amd64_sub_membase_reg */
	"\x0" "bi\xd" "\x0" ""	/* amd64_and_membase_reg */
	"\x0" "bi\xd" "\x0" ""	/* amd64_or_membase_reg */
	"\x0" "bi\xd" "\x0" ""	/* amd64_xor_membase_reg */
	"\x0" "bi\xf" "\x0" ""	/* amd64_mul_membase_reg */
	"\x0" "b\x0" "\x10" "\x0" ""	/* amd64_add_membase_imm */
	"\x0" "b\x0" "\x10" "\x0" ""	/* amd64_sub_membase_imm */
	"\x0" "b\x0" "\xd" "\x0" ""	/* amd64_and_membase_imm */
	"\x0" "b\x0" "\xd" "\x0" ""	/* amd64_or_membase_imm */
	"\x0" "b\x0" "\xd" "\x0" ""	/* amd64_xor_membase_imm */
	"iib\xe" "1"	/* amd64_add_reg_membase */
	"iib\xe" "1"	/* amd64_sub_reg_membase */
	"iib\xe" "1"	/* amd64_and_reg_membase */
	"iib\xe" "1"	/* amd64_or_reg_membase */
	"iib\xe" "1"	/* amd64_xor_reg_membase */
	"iii\xa" "\x0" ""	/* amd64_loadi8_memindex */
	"\x0" "\x0" "\x0" "\x10" "\x0" ""	/* amd64_save_sp_to_lmf */
};

const guint16 amd64_desc_idx [] = {
	0,	/* load */
	0,	/* ldaddr */
	0,	/* store */
	5,	/* nop */
	10,	/* hard_nop */
	15,	/* relaxed_nop */
	0,	/* phi */
	0,	/* fphi */
	0,	/* vphi */
	20,	/* compare */
	25,	/* compare_imm */
	30,	/* fcompare */
	35,	/* lcompare */
	40,	/* icompare */
	45,	/* icompare_imm */
	50,	/* lcompare_imm */
	0,	/* local */
	0,	/* arg */
	55,	/* oparglist */
	0,	/* outarg_vt */
	0,	/* outarg_vtretaddr */
	0,	/* setfret */
	60,	/* setlret */
	65,	/* localloc */
	70,	/* localloc_imm */
	75,	/* checkthis */
	80,	/* voidcall */
	0,	/* voidcallvirt */
	85,	/* voidcall_reg */
	90,	/* voidcall_membase */
	95,	/* call */
	100,	/* call_reg */
	105,	/* call_membase */
	0,	/* callvirt */
	110,	/* fcall */
	0,	/* fcallvirt */
	115,	/* fcall_reg */
	120,	/* fcall_membase */
	125,	/* lcall */
	0,	/* lcallvirt */
	130,	/* lcall_reg */
	135,	/* lcall_membase */
	140,	/* vcall */
	0,	/* vcallvirt */
	145,	/* vcall_reg */
	150,	/* vcall_membase */
	155,	/* vcall2 */
	160,	/* vcall2_reg */
	165,	/* vcall2_membase */
	170,	/* iconst */
	175,	/* i8const */
	180,	/* r4const */
	185,	/* r8const */
	0,	/* regvar */
	0,	/* regoffset */
	0,	/* vtarg_addr */
	190,	/* label */
	0,	/* switch */
	195,	/* throw */
	200,	/* rethrow */
	205,	/* store_membase_reg */
	210,	/* storei1_membase_reg */
	215,	/* storei2_membase_reg */
	220,	/* storei4_membase_reg */
	225,	/* storei8_membase_reg */
	230,	/* storer4_membase_reg */
	235,	/* storer8_membase_reg */
	240,	/* store_membase_imm */
	245,	/* storei1_membase_imm */
	250,	/* storei2_membase_imm */
	255,	/* storei4_membase_imm */
	260,	/* storei8_membase_imm */
	0,	/* storex_membase */
	0,	/* storev_membase */
	265,	/* load_membase */
	270,	/* loadi1_membase */
	275,	/* loadu1_membase */
	280,	/* loadi2_membase */
	285,	/* loadu2_membase */
	290,	/* loadi4_membase */
	295,	/* loadu4_membase */
	300,	/* loadi8_membase */
	305,	/* loadr4_membase */
	310,	/* loadr8_membase */
	0,	/* loadx_membase */
	0,	/* loadv_membase */
	0,	/* load_memindex */
	0,	/* loadi1_memindex */
	0,	/* loadu1_memindex */
	0,	/* loadi2_memindex */
	0,	/* loadu2_memindex */
	0,	/* loadi4_memindex */
	0,	/* loadu4_memindex */
	0,	/* loadi8_memindex */
	0,	/* loadr4_memindex */
	0,	/* loadr8_memindex */
	0,	/* store_memindex */
	0,	/* storei1_memindex */
	0,	/* storei2_memindex */
	0,	/* storei4_memindex */
	0,	/* storei8_memindex */
	0,	/* storer4_memindex */
	0,	/* storer8_memindex */
	315,	/* loadr8_spill_membase */
	320,	/* load_mem */
	325,	/* loadu1_mem */
	330,	/* loadu2_mem */
	335,	/* loadi4_mem */
	340,	/* loadu4_mem */
	345,	/* loadi8_mem */
	0,	/* store_mem_imm */
	350,	/* move */
	0,	/* lmove */
	355,	/* fmove */
	0,	/* vmove */
	0,	/* vzero */
	360,	/* add_imm */
	365,	/* sub_imm */
	370,	/* mul_imm */
	375,	/* div_imm */
	380,	/* div_un_imm */
	385,	/* rem_imm */
	390,	/* rem_un_imm */
	395,	/* and_imm */
	400,	/* or_imm */
	405,	/* xor_imm */
	410,	/* shl_imm */
	415,	/* shr_imm */
	420,	/* shr_un_imm */
	425,	/* br */
	430,	/* jmp */
	435,	/* tailcall */
	440,	/* break */
	445,	/* ceq */
	450,	/* cgt */
	455,	/* cgt.un */
	460,	/* clt */
	465,	/* clt.un */
	470,	/* cond_exc_eq */
	475,	/* cond_exc_ge */
	480,	/* cond_exc_gt */
	485,	/* cond_exc_le */
	490,	/* cond_exc_lt */
	495,	/* cond_exc_ne_un */
	500,	/* cond_exc_ge_un */
	505,	/* cond_exc_gt_un */
	510,	/* cond_exc_le_un */
	515,	/* cond_exc_lt_un */
	520,	/* cond_exc_ov */
	525,	/* cond_exc_no */
	530,	/* cond_exc_c */
	535,	/* cond_exc_nc */
	540,	/* cond_exc_ieq */
	545,	/* cond_exc_ige */
	550,	/* cond_exc_igt */
	555,	/* cond_exc_ile */
	560,	/* cond_exc_ilt */
	565,	/* cond_exc_ine_un */
	570,	/* cond_exc_ige_un */
	575,	/* cond_exc_igt_un */
	580,	/* cond_exc_ile_un */
	585,	/* cond_exc_ilt_un */
	590,	/* cond_exc_iov */
	595,	/* cond_exc_ino */
	600,	/* cond_exc_ic */
	605,	/* cond_exc_inc */
	610,	/* long_add */
	615,	/* long_sub */
	620,	/* long_mul */
	625,	/* long_div */
	630,	/* long_div_un */
	635,	/* long_rem */
	640,	/* long_rem_un */
	645,	/* long_and */
	650,	/* long_or */
	655,	/* long_xor */
	660,	/* long_shl */
	665,	/* long_shr */
	670,	/* long_shr_un */
	675,	/* long_neg */
	680,	/* long_not */
	685,	/* long_conv_to_i1 */
	690,	/* long_conv_to_i2 */
	695,	/* long_conv_to_i4 */
	700,	/* long_conv_to_i8 */
	705,	/* long_conv_to_r4 */
	710,	/* long_conv_to_r8 */
	715,	/* long_conv_to_u4 */
	720,	/* long_conv_to_u8 */
	725,	/* long_conv_to_u2 */
	730,	/* long_conv_to_u1 */
	0,	/* long_conv_to_i */
	735,	/* long_conv_to_ovf_i */
	0,	/* long_conv_to_ovf_u */
	0,	/* long_add_ovf */
	0,	/* long_add_ovf_un */
	740,	/* long_mul_ovf */
	745,	/* long_mul_ovf_un */
	0,	/* long_sub_ovf */
	0,	/* long_sub_ovf_un */
	0,	/* long_conv_to_ovf_i1_un */
	0,	/* long_conv_to_ovf_i2_un */
	750,	/* long_conv_to_ovf_i4_un */
	0,	/* long_conv_to_ovf_i8_un */
	0,	/* long_conv_to_ovf_u1_un */
	0,	/* long_conv_to_ovf_u2_un */
	0,	/* long_conv_to_ovf_u4_un */
	0,	/* long_conv_to_ovf_u8_un */
	0,	/* long_conv_to_ovf_i_un */
	0,	/* long_conv_to_ovf_u_un */
	0,	/* long_conv_to_ovf_i1 */
	0,	/* long_conv_to_ovf_u1 */
	0,	/* long_conv_to_ovf_i2 */
	0,	/* long_conv_to_ovf_u2 */
	0,	/* long_conv_to_ovf_i4 */
	755,	/* long_conv_to_ovf_u4 */
	0,	/* long_conv_to_ovf_i8 */
	0,	/* long_conv_to_ovf_u8 */
	760,	/* long_ceq */
	765,	/* long_cgt */
	770,	/* long_cgt_un */
	775,	/* long_clt */
	780,	/* long_clt_un */
	785,	/* long_conv_to_r_un */
	0,	/* long_conv_to_u */
	790,	/* long_add_imm */
	795,	/* long_sub_imm */
	800,	/* long_mul_imm */
	805,	/* long_and_imm */
	810,	/* long_or_imm */
	815,	/* long_xor_imm */
	820,	/* long_shl_imm */
	825,	/* long_shr_imm */
	830,	/* long_shr_un_imm */
	0,	/* long_div_imm */
	0,	/* long_div_un_imm */
	0,	/* long_rem_imm */
	0,	/* long_rem_un_imm */
	835,	/* long_beq */
	840,	/* long_bge */
	845,	/* long_bgt */
	850,	/* long_ble */
	855,	/* long_blt */
	860,	/* long_bne_un */
	865,	/* long_bge_un */
	870,	/* long_bgt_un */
	875,	/* long_ble_un */
	880,	/* long_blt_un */
	0,	/* long_conv_to_r8_2 */
	0,	/* long_conv_to_r4_2 */
	0,	/* long_conv_to_r_un_2 */
	0,	/* long_conv_to_ovf_i4_2 */
	885,	/* int_add */
	890,	/* int_sub */
	895,	/* int_mul */
	900,	/* int_div */
	905,	/* int_div_un */
	910,	/* int_rem */
	915,	/* int_rem_un */
	920,	/* int_and */
	925,	/* int_or */
	930,	/* int_xor */
	935,	/* int_shl */
	940,	/* int_shr */
	945,	/* int_shr_un */
	950,	/* int_neg */
	955,	/* int_not */
	960,	/* int_conv_to_i1 */
	965,	/* int_conv_to_i2 */
	970,	/* int_conv_to_i4 */
	975,	/* int_conv_to_i8 */
	980,	/* int_conv_to_r4 */
	985,	/* int_conv_to_r8 */
	990,	/* int_conv_to_u4 */
	995,	/* int_conv_to_u8 */
	0,	/* int_conv_to_r_un */
	1000,	/* int_conv_to_u */
	1005,	/* int_conv_to_u2 */
	1010,	/* int_conv_to_u1 */
	1015,	/* int_conv_to_i */
	0,	/* int_conv_to_ovf_i */
	0,	/* int_conv_to_ovf_u */
	0,	/* int_add_ovf */
	0,	/* int_add_ovf_un */
	1020,	/* int_mul_ovf */
	1025,	/* int_mul_ovf_un */
	0,	/* int_sub_ovf */
	0,	/* int_sub_ovf_un */
	0,	/* int_conv_to_ovf_i1_un */
	0,	/* int_conv_to_ovf_i2_un */
	0,	/* int_conv_to_ovf_i4_un */
	0,	/* int_conv_to_ovf_i8_un */
	0,	/* int_conv_to_ovf_u1_un */
	0,	/* int_conv_to_ovf_u2_un */
	0,	/* int_conv_to_ovf_u4_un */
	0,	/* int_conv_to_ovf_u8_un */
	0,	/* int_conv_to_ovf_i_un */
	0,	/* int_conv_to_ovf_u_un */
	0,	/* int_conv_to_ovf_i1 */
	0,	/* int_conv_to_ovf_u1 */
	0,	/* int_conv_to_ovf_i2 */
	0,	/* int_conv_to_ovf_u2 */
	0,	/* int_conv_to_ovf_i4 */
	0,	/* int_conv_to_ovf_u4 */
	0,	/* int_conv_to_ovf_i8 */
	0,	/* int_conv_to_ovf_u8 */
	1030,	/* int_adc */
	1035,	/* int_adc_imm */
	1040,	/* int_sbb */
	1045,	/* int_sbb_imm */
	1050,	/* int_addcc */
	1055,	/* int_subcc */
	1060,	/* int_add_imm */
	1065,	/* int_sub_imm */
	1070,	/* int_mul_imm */
	1075,	/* int_div_imm */
	1080,	/* int_div_un_imm */
	1085,	/* int_rem_imm */
	1090,	/* int_rem_un_imm */
	1095,	/* int_and_imm */
	1100,	/* int_or_imm */
	1105,	/* int_xor_imm */
	1110,	/* int_shl_imm */
	1115,	/* int_shr_imm */
	1120,	/* int_shr_un_imm */
	1125,	/* int_ceq */
	1130,	/* int_cgt */
	1135,	/* int_cgt_un */
	1140,	/* int_clt */
	1145,	/* int_clt_un */
	1150,	/* int_beq */
	1155,	/* int_bge */
	1160,	/* int_bgt */
	1165,	/* int_ble */
	1170,	/* int_blt */
	1175,	/* int_bne_un */
	1180,	/* int_bge_un */
	1185,	/* int_bgt_un */
	1190,	/* int_ble_un */
	1195,	/* int_blt_un */
	1200,	/* float_beq */
	1205,	/* float_bge */
	1210,	/* float_bgt */
	1215,	/* float_ble */
	1220,	/* float_blt */
	1225,	/* float_bne_un */
	1230,	/* float_bge_un */
	1235,	/* float_bgt_un */
	1240,	/* float_ble_un */
	1245,	/* float_blt_un */
	1250,	/* float_add */
	1255,	/* float_sub */
	1260,	/* float_mul */
	1265,	/* float_div */
	1270,	/* float_div_un */
	1275,	/* float_rem */
	1280,	/* float_rem_un */
	1285,	/* float_neg */
	1290,	/* float_not */
	1295,	/* float_conv_to_i1 */
	1300,	/* float_conv_to_i2 */
	1305,	/* float_conv_to_i4 */
	1310,	/* float_conv_to_i8 */
	1315,	/* float_conv_to_r4 */
	0,	/* float_conv_to_r8 */
	1320,	/* float_conv_to_u4 */
	1325,	/* float_conv_to_u8 */
	1330,	/* float_conv_to_u2 */
	1335,	/* float_conv_to_u1 */
	1340,	/* float_conv_to_i */
	1345,	/* float_conv_to_ovf_i */
	1350,	/* float_conv_to_ovd_u */
	0,	/* float_add_ovf */
	0,	/* float_add_ovf_un */
	1355,	/* float_mul_ovf */
	0,	/* float_mul_ovf_un */
	0,	/* float_sub_ovf */
	0,	/* float_sub_ovf_un */
	0,	/* float_conv_to_ovf_i1_un */
	0,	/* float_conv_to_ovf_i2_un */
	0,	/* float_conv_to_ovf_i4_un */
	0,	/* float_conv_to_ovf_i8_un */
	0,	/* float_conv_to_ovf_u1_un */
	0,	/* float_conv_to_ovf_u2_un */
	0,	/* float_conv_to_ovf_u4_un */
	0,	/* float_conv_to_ovf_u8_un */
	0,	/* float_conv_to_ovf_i_un */
	0,	/* float_conv_to_ovf_u_un */
	0,	/* float_conv_to_ovf_i1 */
	0,	/* float_conv_to_ovf_u1 */
	0,	/* float_conv_to_ovf_i2 */
	0,	/* float_conv_to_ovf_u2 */
	0,	/* float_conv_to_ovf_i4 */
	0,	/* float_conv_to_ovf_u4 */
	0,	/* float_conv_to_ovf_i8 */
	0,	/* float_conv_to_ovf_u8 */
	1360,	/* float_ceq */
	1365,	/* float_cgt */
	1370,	/* float_cgt_un */
	1375,	/* float_clt */
	1380,	/* float_clt_un */
	1385,	/* float_ceq_membase */
	1390,	/* float_cgt_membase */
	1395,	/* float_cgt_un_membase */
	1400,	/* float_clt_membase */
	1405,	/* float_clt_un_membase */
	1410,	/* float_conv_to_u */
	1415,	/* ckfinite */
	0,	/* float_getlow32 */
	0,	/* float_gethigh32 */
	1420,	/* jump_table */
	1425,	/* aot_const */
	0,	/* patch_info */
	0,	/* got_entry */
	1430,	/* call_handler */
	1435,	/* start_handler */
	1440,	/* endfilter */
	1445,	/* endfinally */
	1450,	/* bigmul */
	1455,	/* bigmul_un */
	1460,	/* int_min_un */
	1465,	/* int_max_un */
	1470,	/* long_min_un */
	1475,	/* long_max_un */
	0,	/* min */
	0,	/* max */
	1480,	/* int_min */
	1485,	/* int_max */
	1490,	/* long_min */
	1495,	/* long_max */
	1500,	/* adc */
	1505,	/* adc_imm */
	1510,	/* sbb */
	1515,	/* sbb_imm */
	1520,	/* addcc */
	0,	/* addcc_imm */
	1525,	/* subcc */
	0,	/* subcc_imm */
	1530,	/* br_reg */
	1535,	/* sext_i1 */
	1540,	/* sext_i2 */
	1545,	/* sext_i4 */
	0,	/* zext_i1 */
	0,	/* zext_i2 */
	1550,	/* zext_i4 */
	0,	/* cne */
	0,	/* add_ovf_carry */
	0,	/* sub_ovf_carry */
	0,	/* add_ovf_un_carry */
	0,	/* sub_ovf_un_carry */
	1555,	/* sin */
	1560,	/* cos */
	1565,	/* abs */
	1570,	/* tan */
	1575,	/* atan */
	1580,	/* sqrt */
	0,	/* round */
	0,	/* strlen */
	0,	/* newarr */
	0,	/* ldlen */
	0,	/* bounds_check */
	0,	/* getldelema2 */
	0,	/* memcpy */
	0,	/* memset */
	0,	/* save_lmf */
	0,	/* restore_lmf */
	1585,	/* tls_get */
	0,	/* load_gotaddr */
	1590,	/* dummy_use */
	1595,	/* dummy_store */
	1600,	/* not_reached */
	1605,	/* not_null */
	0,	/* xmove */
	0,	/* xzero */
	0,	/* xphi */
	1610,	/* atomic_add_i4 */
	1615,	/* atomic_add_new_i4 */
	0,	/* atomic_add_imm_i4 */
	0,	/* atomic_add_imm_new_i4 */
	1620,	/* atomic_exchange_i4 */
	1625,	/* atomic_add_i8 */
	1630,	/* atomic_add_new_i8 */
	0,	/* atomic_add_imm_i8 */
	0,	/* atomic_add_imm_new_i8 */
	1635,	/* atomic_exchange_i8 */
	1640,	/* memory_barrier */
	1645,	/* atomic_cas_imm_i4 */
	1650,	/* atomic_cas_imm_i8 */
	1655,	/* cmov_ieq */
	1660,	/* cmov_ige */
	1665,	/* cmov_igt */
	1670,	/* cmov_ile */
	1675,	/* cmov_ilt */
	1680,	/* cmov_ine_un */
	1685,	/* cmov_ige_un */
	1690,	/* cmov_igt_un */
	1695,	/* cmov_ile_un */
	1700,	/* cmov_ilt_un */
	1705,	/* cmov_leq */
	1710,	/* cmov_lge */
	1715,	/* cmov_lgt */
	1720,	/* cmov_lle */
	1725,	/* cmov_llt */
	1730,	/* cmov_lne_un */
	1735,	/* cmov_lge_un */
	1740,	/* cmov_lgt_un */
	1745,	/* cmov_lle_un */
	1750,	/* cmov_llt_un */
	1755,	/* liverange_start */
	1760,	/* liverange_end */
	1765,	/* x86_test_null */
	1770,	/* x86_compare_membase_reg */
	1775,	/* x86_compare_membase_imm */
	0,	/* x86_compare_mem_imm */
	1780,	/* x86_compare_membase8_imm */
	1785,	/* x86_compare_reg_membase */
	1790,	/* x86_inc_reg */
	1795,	/* x86_inc_membase */
	1800,	/* x86_dec_reg */
	1805,	/* x86_dec_membase */
	1810,	/* x86_add_membase_imm */
	1815,	/* x86_sub_membase_imm */
	1820,	/* x86_and_membase_imm */
	1825,	/* x86_or_membase_imm */
	1830,	/* x86_xor_membase_imm */
	1835,	/* x86_add_membase_reg */
	1840,	/* x86_sub_membase_reg */
	1845,	/* x86_and_membase_reg */
	1850,	/* x86_or_membase_reg */
	1855,	/* x86_xor_membase_reg */
	1860,	/* x86_mul_membase_reg */
	1865,	/* x86_add_reg_membase */
	1870,	/* x86_sub_reg_membase */
	1875,	/* x86_mul_reg_membase */
	1880,	/* x86_and_reg_membase */
	1885,	/* x86_or_reg_membase */
	1890,	/* x86_xor_reg_membase */
	1895,	/* x86_push_membase */
	1900,	/* x86_push_imm */
	1905,	/* x86_push */
	1910,	/* x86_push_obj */
	0,	/* x86_push_got_entry */
	1915,	/* x86_lea */
	1920,	/* x86_lea_membase */
	1925,	/* x86_xchg */
	1930,	/* x86_fpop */
	0,	/* x86_fp_load_i8 */
	0,	/* x86_fp_load_i4 */
	1935,	/* x86_seteq_membase */
	0,	/* x86_setne_membase */
	0,	/* x86_fxch */
	1940,	/* amd64_test_null */
	1945,	/* amd64_set_xmmreg_r4 */
	1950,	/* amd64_set_xmmreg_r8 */
	1955,	/* amd64_icompare_membase_reg */
	1960,	/* amd64_icompare_membase_imm */
	1965,	/* amd64_icompare_reg_membase */
	1970,	/* amd64_compare_membase_reg */
	1975,	/* amd64_compare_membase_imm */
	1980,	/* amd64_compare_reg_membase */
	1985,	/* amd64_add_membase_reg */
	1990,	/* amd64_sub_membase_reg */
	1995,	/* amd64_and_membase_reg */
	2000,	/* amd64_or_membase_reg */
	2005,	/* amd64_xor_membase_reg */
	2010,	/* amd64_mul_membase_reg */
	2015,	/* amd64_add_membase_imm */
	2020,	/* amd64_sub_membase_imm */
	2025,	/* amd64_and_membase_imm */
	2030,	/* amd64_or_membase_imm */
	2035,	/* amd64_xor_membase_imm */
	0,	/* amd64_mul_membase_imm */
	2040,	/* amd64_add_reg_membase */
	2045,	/* amd64_sub_reg_membase */
	2050,	/* amd64_and_reg_membase */
	2055,	/* amd64_or_reg_membase */
	2060,	/* amd64_xor_reg_membase */
	0,	/* amd64_mul_reg_membase */
	2065,	/* amd64_loadi8_memindex */
	2070,	/* amd64_save_sp_to_lmf */
};

