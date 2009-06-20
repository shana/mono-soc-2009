/*
 * mini-mips.c: MIPS backend for the Mono code generator
 *
 * Authors:
 *   Mark Mason (mason@broadcom.com)
 *
 * Based on mini-ppc.c by
 *   Paolo Molaro (lupus@ximian.com)
 *   Dietmar Maurer (dietmar@ximian.com)
 *
 * (C) 2006 Broadcom
 * (C) 2003 Ximian, Inc.
 */
#include "mini.h"
#include <string.h>
#include <asm/cachectl.h>

#include <mono/metadata/appdomain.h>
#include <mono/metadata/debug-helpers.h>

#include <mono/arch/mips/mips-codegen.h>

#include "mini-mips.h"
#include "cpu-mips.h"
#include "trace.h"
#include "ir-emit.h"

#define SAVE_FP_REGS		0
#define SAVE_ALL_REGS		0
#define EXTRA_STACK_SPACE	0	/* suppresses some s-reg corruption issues */
#define LONG_BRANCH		1	/* needed for yyparse in mcs */

#define SAVE_LMF		1
#define ALWAYS_USE_FP		1
#define ALWAYS_SAVE_RA		1	/* call-handler & switch currently clobber ra */

#define PROMOTE_R4_TO_R8	1	/* promote single values in registers to doubles */
#define USE_MUL			1	/* use mul instead of mult/mflo for multiply */

enum {
	TLS_MODE_DETECT,
	TLS_MODE_FAILED,
	TLS_MODE_LTHREADS,
	TLS_MODE_NPTL
};

/* This mutex protects architecture specific caches */
#define mono_mini_arch_lock() EnterCriticalSection (&mini_arch_mutex)
#define mono_mini_arch_unlock() LeaveCriticalSection (&mini_arch_mutex)
static CRITICAL_SECTION mini_arch_mutex;

int mono_exc_esp_offset = 0;
static int tls_mode = TLS_MODE_DETECT;
static int lmf_pthread_key = -1;
static int monothread_key = -1;
static int monodomain_key = -1;

#undef DEBUG
#define DEBUG(a) if (cfg->verbose_level > 1) a
#undef DEBUG
#define DEBUG(a) a
#undef DEBUG
#define DEBUG(a)

#define EMIT_SYSTEM_EXCEPTION_NAME(exc_name)            \
        do {                                                        \
		code = mips_emit_exc_by_name (code, exc_name);	\
		cfg->bb_exit->max_offset += 16;				\
	} while (0) 


#define emit_linuxthreads_tls(code,dreg,key) do {\
		int off1, off2;	\
		off1 = offsets_from_pthread_key ((key), &off2);	\
		g_assert_not_reached ();		\
		ppc_lwz ((code), (dreg), off1, ppc_r2);	\
		ppc_lwz ((code), (dreg), off2, (dreg));	\
	} while (0);


#define emit_tls_access(code,dreg,key) do {	\
		switch (tls_mode) {	\
		case TLS_MODE_LTHREADS: emit_linuxthreads_tls(code,dreg,key); break;	\
		default: g_assert_not_reached ();	\
		}	\
	} while (0)

#define MONO_EMIT_NEW_LOAD_R8(cfg,dr,addr) do { \
		MonoInst *inst;				   \
		MONO_INST_NEW ((cfg), (inst), OP_R8CONST); \
		inst->type = STACK_R8;			   \
		inst->dreg = (dr);		       \
		inst->inst_p0 = (void*)(addr);	       \
		mono_bblock_add_inst (cfg->cbb, inst); \
	} while (0)

#define ins_is_compare(ins) ((ins) && (((ins)->opcode == OP_COMPARE) \
				       || ((ins)->opcode == OP_ICOMPARE) \
				       || ((ins)->opcode == OP_LCOMPARE)))
#define ins_is_compare_imm(ins) ((ins) && (((ins)->opcode == OP_COMPARE_IMM) \
					   || ((ins)->opcode == OP_ICOMPARE_IMM) \
					   || ((ins)->opcode == OP_LCOMPARE_IMM)))

#define INS_REWRITE(ins, op, _s1, _s2)	do { \
			int s1 = _s1;			\
			int s2 = _s2;			\
			ins->opcode = (op);		\
			ins->sreg1 = (s1);		\
			ins->sreg2 = (s2);		\
	} while (0);

#define INS_REWRITE_IMM(ins, op, _s1, _imm)	do { \
			int s1 = _s1;			\
			ins->opcode = (op);		\
			ins->sreg1 = (s1);		\
			ins->inst_imm = (_imm);		\
	} while (0);


typedef struct InstList InstList;

struct InstList {
	InstList *prev;
	InstList *next;
	MonoInst *data;
};

enum {
	RegTypeGeneral,
	RegTypeBase,
	RegTypeFP,
	RegTypeStructByVal,
	RegTypeStructByAddr
};

typedef struct {
	gint32  offset;
	guint16 vtsize; /* in param area */
	guint8  reg;
	guint8  regtype : 4; /* 0 general, 1 basereg, 2 floating point register, see RegType* */
	guint8  size    : 4; /* 1, 2, 4, 8, or regs used by RegTypeStructByVal */
} ArgInfo;

typedef struct {
	int nargs;
	int gr;
	int fr;
	gboolean gr_passed;
	gboolean on_stack;
	int stack_size;
	guint32 stack_usage;
	guint32 struct_ret;
	ArgInfo ret;
	ArgInfo sig_cookie;
	ArgInfo args [1];
} CallInfo;

void patch_lui_addiu(guint32 *ip, guint32 val);
guint8 *mono_arch_emit_epilog_sub (MonoCompile *cfg, guint8 *code);
guint8 *mips_emit_cond_branch (MonoCompile *cfg, guint8 *code, int op, MonoInst *ins);
void mips_adjust_stackframe(MonoCompile *cfg);
void mono_arch_emit_this_vret_args (MonoCompile *cfg, MonoCallInst *inst, int this_reg, int this_type, int vt_reg);
MonoInst *mono_arch_get_inst_for_method (MonoCompile *cfg, MonoMethod *cmethod, MonoMethodSignature *fsig, MonoInst **args);


void
mono_arch_flush_icache (guint8 *code, gint size)
{
	/* Linux/MIPS specific */
	cacheflush (code, size, BCACHE);
}

void
mono_arch_flush_register_windows (void)
{
}

gboolean 
mono_arch_is_inst_imm (gint64 imm)
{
	return TRUE;
}

static guint8 *
mips_emit_exc_by_name(guint8 *code, const char *name)
{
	guint32 addr;
	MonoClass *exc_class;

	exc_class = mono_class_from_name (mono_defaults.corlib, "System", name);
	g_assert (exc_class);

	mips_load_const (code, mips_a0, exc_class->type_token);
	addr = (guint32) mono_arch_get_throw_corlib_exception ();
	mips_load_const (code, mips_t9, addr);
	mips_jalr (code, mips_t9, mips_ra);
	mips_nop (code);

	return code;
}


guint8 *
mips_emit_load_const(guint8 *code, int dreg, mgreg_t v)
{
	if (mips_is_imm16 (v))
		mips_addiu (code, dreg, mips_zero, ((guint32)v) & 0xffff);
	else {
#ifdef SIZEOF_REGISTER == 8
		if (v != (long) v) {
			/* v is not a sign-extended 32-bit value */
			mips_lui (code, dreg, mips_zero, (guint32)((v >> (32+16)) & 0xffff));
			mips_ori (code, dreg, dreg, (guint32)((v >> (32)) & 0xffff));
			mips_dsll (code, dreg, dreg, 16);
			mips_ori (code, dreg, dreg, (guint32)((v >> (16)) & 0xffff));
			mips_dsll (code, dreg, dreg, 16);
			mips_ori (code, dreg, dreg, (guint32)(v & 0xffff));
			return code;
		}
#endif
		if (((guint32)v) & (1 << 15)) {
			mips_lui (code, dreg, mips_zero, (((guint32)v)>>16)+1);
		}
		else {
			mips_lui (code, dreg, mips_zero, (((guint32)v)>>16));
		}
		if (((guint32)v) & 0xffff)
			mips_addiu (code, dreg, dreg, ((guint32)v) & 0xffff);
	}
	return code;
}

guint8 *
mips_emit_cond_branch (MonoCompile *cfg, guint8 *code, int op, MonoInst *ins)
{
#if LONG_BRANCH
	int br_offset = 5;
#endif

	g_assert (ins);
#if LONG_BRANCH
	/* Invert test and emit branch around jump */
	switch (op) {
	case OP_MIPS_BEQ:
		mips_bne (code, ins->sreg1, ins->sreg2, br_offset);
		mips_nop (code);
		break;
	case OP_MIPS_BNE:
		mips_beq (code, ins->sreg1, ins->sreg2, br_offset);
		mips_nop (code);
		break;
	case OP_MIPS_BGEZ:
		mips_bltz (code, ins->sreg1, br_offset);
		mips_nop (code);
		break;
	case OP_MIPS_BGTZ:
		mips_blez (code, ins->sreg1, br_offset);
		mips_nop (code);
		break;
	case OP_MIPS_BLEZ:
		mips_bgtz (code, ins->sreg1, br_offset);
		mips_nop (code);
		break;
	case OP_MIPS_BLTZ:
		mips_bgez (code, ins->sreg1, br_offset);
		mips_nop (code);
		break;
	default:
		g_assert_not_reached ();
	}
	mono_add_patch_info (cfg, code - cfg->native_code,
			     MONO_PATCH_INFO_BB, ins->inst_true_bb);
	mips_lui (code, mips_at, mips_zero, 0);
	mips_addiu (code, mips_at, mips_at, 0);
	mips_jr (code, mips_at);
	mips_nop (code);
#else
	mono_add_patch_info (cfg, code - cfg->native_code,
			     MONO_PATCH_INFO_BB, ins->inst_true_bb);
	switch (op) {
	case OP_MIPS_BEQ:
		mips_beq (code, ins->sreg1, ins->sreg2, 0);
		mips_nop (code);
		break;
	case OP_MIPS_BNE:
		mips_bne (code, ins->sreg1, ins->sreg2, 0);
		mips_nop (code);
		break;
	case OP_MIPS_BGEZ:
		mips_bgez (code, ins->sreg1, 0);
		mips_nop (code);
		break;
	case OP_MIPS_BGTZ:
		mips_bgtz (code, ins->sreg1, 0);
		mips_nop (code);
		break;
	case OP_MIPS_BLEZ:
		mips_blez (code, ins->sreg1, 0);
		mips_nop (code);
		break;
	case OP_MIPS_BLTZ:
		mips_bltz (code, ins->sreg1, 0);
		mips_nop (code);
		break;
	default:
		g_assert_not_reached ();
	}
#endif
	return (code);
}

/* XXX - big-endian dependent? */
void
patch_lui_addiu(guint32 *ip, guint32 val)
{
	guint16 *__lui_addiu = (guint16*)(void *)(ip);

#if 0
	printf ("patch_lui_addiu ip=0x%08x (0x%08x, 0x%08x) to point to 0x%08x\n",
		ip, ((guint32 *)ip)[0], ((guint32 *)ip)[1], val);
	fflush (stdout);
#endif
	if (((guint32)(val)) & (1 << 15))
		__lui_addiu [MINI_LS_WORD_IDX] = ((((guint32)(val)) >> 16) & 0xffff) + 1;
	else
		__lui_addiu [MINI_LS_WORD_IDX] = (((guint32)(val)) >> 16) & 0xffff;
	__lui_addiu [MINI_LS_WORD_IDX + 2] = ((guint32)(val)) & 0xffff;
	mono_arch_flush_icache ((guint8 *)ip, 8);
}

guint32 trap_target;
void
mips_patch (guint32 *code, guint32 target)
{
	guint32 ins = *code;
	guint32 op = ins >> 26;
	guint32 diff, offset;

	g_assert (trap_target != target);
	//printf ("patching 0x%08x (0x%08x) to point to 0x%08x\n", code, ins, target);
	switch (op) {
	case 0x00: /* jr ra */
		if (ins == 0x3e00008)
			break;
		g_assert_not_reached ();
		break;
	case 0x02: /* j */
	case 0x03: /* jal */
		g_assert (!(target & 0x03));
		g_assert ((target & 0xfc000000) == (((guint32)code) & 0xfc000000));
		ins = (ins & 0xfc000000) | (((target) >> 2) & 0x03ffffff);
		*code = ins;
		mono_arch_flush_icache ((guint8 *)code, 4);
		break;
	case 0x01: /* BLTZ */
	case 0x04: /* BEQ */
	case 0x05: /* BNE */
	case 0x06: /* BLEZ */
	case 0x07: /* BGTZ */
	case 0x11: /* bc1t */
		diff = target - (guint32)(code + 1);
		g_assert (((diff & 0x0003ffff) == diff) || ((diff | 0xfffc0000) == diff));
		g_assert (!(diff & 0x03));
		offset = ((gint32)diff) >> 2;
		g_assert (((int)offset) == ((int)(short)offset));
		ins = (ins & 0xffff0000) | (offset & 0x0000ffff);
		*code = ins;
		mono_arch_flush_icache ((guint8 *)code, 4);
		break;
	case 0x0f: /* LUI / ADDIU pair */
		patch_lui_addiu (code, target);
		mono_arch_flush_icache ((guint8 *)code, 8);
		break;

	default:
		printf ("unknown op 0x%02x (0x%08x) @ %p\n", op, ins, code);
		g_assert_not_reached ();
	}
}

#if 0
static int
offsets_from_pthread_key (guint32 key, int *offset2)
{
	int idx1 = key / 32;
	int idx2 = key % 32;
	*offset2 = idx2 * sizeof (gpointer);
	return 284 + idx1 * sizeof (gpointer);
}
#endif

const char*
mono_arch_regname (int reg) {
#if _MIPS_SIM == _ABIO32
	static const char * rnames[] = {
		"zero", "at", "v0", "v1",
		"a0", "a1", "a2", "a3",
		"t0", "t1", "t2", "t3",
		"t4", "t5", "t6", "t7",
		"s0", "s1", "s2", "s3",
		"s4", "s5", "s6", "s7",
		"t8", "t9", "k0", "k1",
		"gp", "sp", "fp", "ra"
	};
#elif _MIPS_SIM == _ABIN32
	static const char * rnames[] = {
		"zero", "at", "v0", "v1",
		"a0", "a1", "a2", "a3",
		"a4", "a5", "a6", "a7",
		"t0", "t1", "t2", "t3",
		"s0", "s1", "s2", "s3",
		"s4", "s5", "s6", "s7",
		"t8", "t9", "k0", "k1",
		"gp", "sp", "fp", "ra"
	};
#endif
	if (reg >= 0 && reg < 32)
		return rnames [reg];
	return "unknown";
}

const char*
mono_arch_fregname (int reg) {
	static const char * rnames[] = {
		"f0", "f1", "f2", "f3",
		"f4", "f5", "f6", "f7",
		"f8", "f9", "f10", "f11",
		"f12", "f13", "f14", "f15",
		"f16", "f17", "f18", "f19",
		"f20", "f21", "f22", "f23",
		"f24", "f25", "f26", "f27",
		"f28", "f29", "f30", "f31"
	};
	if (reg >= 0 && reg < 32)
		return rnames [reg];
	return "unknown";
}

/* this function overwrites at */
static guint8*
emit_memcpy (guint8 *code, int size, int dreg, int doffset, int sreg, int soffset)
{
	/* XXX write a loop, not an unrolled loop */
	while (size > 0) {
		mips_lw (code, mips_at, sreg, soffset);
		mips_sw (code, mips_at, dreg, doffset);
		size -= 4;
		soffset += 4;
		doffset += 4;
	}
	return code;
}

/*
 * mono_arch_get_argument_info:
 * @csig:  a method signature
 * @param_count: the number of parameters to consider
 * @arg_info: an array to store the result infos
 *
 * Gathers information on parameters such as size, alignment and
 * padding. arg_info should be large enought to hold param_count + 1 entries. 
 *
 * Returns the size of the activation frame.
 */
int
mono_arch_get_argument_info (MonoMethodSignature *csig, int param_count, MonoJitArgumentInfo *arg_info)
{
	int k, frame_size = 0;
	guint32 size, align, pad;
	int offset = 0;

	if (MONO_TYPE_ISSTRUCT (csig->ret)) { 
		frame_size += sizeof (gpointer);
		offset += 4;
	}

	arg_info [0].offset = offset;

	if (csig->hasthis) {
		frame_size += sizeof (gpointer);
		offset += 4;
	}

	arg_info [0].size = frame_size;

	for (k = 0; k < param_count; k++) {
		size = mini_type_stack_size_full (NULL, csig->params [k], &align, csig->pinvoke);

		/* ignore alignment for now */
		align = 1;

		frame_size += pad = (align - (frame_size & (align - 1))) & (align - 1);	
		arg_info [k].pad = pad;
		frame_size += size;
		arg_info [k + 1].pad = 0;
		arg_info [k + 1].size = size;
		offset += pad;
		arg_info [k + 1].offset = offset;
		offset += size;
	}

	align = MONO_ARCH_FRAME_ALIGNMENT;
	frame_size += pad = (align - (frame_size & (align - 1))) & (align - 1);
	arg_info [k].pad = pad;

	return frame_size;
}


gpointer
mono_arch_get_this_arg_from_call (MonoGenericSharingContext *gsctx, MonoMethodSignature *sig, gssize *regs, guint8 *code)
{
	/* FIXME: handle returning a struct */
	if (MONO_TYPE_ISSTRUCT (sig->ret))
		return (gpointer)regs [mips_a1];
	return (gpointer)regs [mips_a0];
}

/*
 * Initialize the cpu to execute managed code.
 */
void
mono_arch_cpu_init (void)
{
}

/*
 * Initialize architecture specific code.
 */
void
mono_arch_init (void)
{
	InitializeCriticalSection (&mini_arch_mutex);	
}

/*
 * Cleanup architecture specific code.
 */
void
mono_arch_cleanup (void)
{
	DeleteCriticalSection (&mini_arch_mutex);
}

/*
 * This function returns the optimizations supported on this cpu.
 */
guint32
mono_arch_cpu_optimizazions (guint32 *exclude_mask)
{
	guint32 opts = 0;

	/* no mips-specific optimizations yet */
	*exclude_mask = 0;
	return opts;
}

static gboolean
is_regsize_var (MonoType *t) {
	if (t->byref)
		return TRUE;
	t = mono_type_get_underlying_type (t);
	switch (t->type) {
	case MONO_TYPE_I4:
	case MONO_TYPE_U4:
#if (SIZEOF_REGISTER == 8)
	case MONO_TYPE_I8:
	case MONO_TYPE_U8:
#endif
	case MONO_TYPE_I:
	case MONO_TYPE_U:
	case MONO_TYPE_PTR:
	case MONO_TYPE_FNPTR:
		return TRUE;
	case MONO_TYPE_OBJECT:
	case MONO_TYPE_STRING:
	case MONO_TYPE_CLASS:
	case MONO_TYPE_SZARRAY:
	case MONO_TYPE_ARRAY:
		return TRUE;
	case MONO_TYPE_GENERICINST:
		if (!mono_type_generic_inst_is_valuetype (t))
			return TRUE;
		return FALSE;
	case MONO_TYPE_VALUETYPE:
		return FALSE;
	}
	return FALSE;
}

GList *
mono_arch_get_allocatable_int_vars (MonoCompile *cfg)
{
	GList *vars = NULL;
	int i;

	for (i = 0; i < cfg->num_varinfo; i++) {
		MonoInst *ins = cfg->varinfo [i];
		MonoMethodVar *vmv = MONO_VARINFO (cfg, i);

		/* unused vars */
		if (vmv->range.first_use.abs_pos >= vmv->range.last_use.abs_pos)
			continue;

		if (ins->flags & (MONO_INST_VOLATILE|MONO_INST_INDIRECT) || (ins->opcode != OP_LOCAL && ins->opcode != OP_ARG))
			continue;

		/* we can only allocate 32 bit values */
		if (is_regsize_var (ins->inst_vtype)) {
			g_assert (MONO_VARINFO (cfg, i)->reg == -1);
			g_assert (i == vmv->idx);
			vars = mono_varlist_insert_sorted (cfg, vars, vmv, FALSE);
		}
	}

	return vars;
}

GList *
mono_arch_get_global_int_regs (MonoCompile *cfg)
{
	GList *regs = NULL;

	regs = g_list_prepend (regs, (gpointer)mips_s0);
	regs = g_list_prepend (regs, (gpointer)mips_s1);
	regs = g_list_prepend (regs, (gpointer)mips_s2);
	regs = g_list_prepend (regs, (gpointer)mips_s3);
	regs = g_list_prepend (regs, (gpointer)mips_s4);
	//regs = g_list_prepend (regs, (gpointer)mips_s5);
	regs = g_list_prepend (regs, (gpointer)mips_s6);
	regs = g_list_prepend (regs, (gpointer)mips_s7);

	return regs;
}

/*
 * mono_arch_regalloc_cost:
 *
 * Return the cost, in number of memory references, of the action of 
 * allocating the variable VMV into a register during global register
 * allocation.
 */
guint32
mono_arch_regalloc_cost (MonoCompile *cfg, MonoMethodVar *vmv)
{
	/* FIXME: */
	return 2;
}

static void
args_onto_stack (CallInfo *info)
{
	g_assert (!info->on_stack);
	g_assert (info->stack_size <= MIPS_STACK_PARAM_OFFSET);
	info->on_stack = TRUE;
	info->stack_size = MIPS_STACK_PARAM_OFFSET;
}

#if _MIPS_SIM == _ABIO32
/*
 * O32 calling convention version
 */

static void
add_int32_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
	}
	else {
		ainfo->regtype = RegTypeGeneral;
		ainfo->reg = info->gr;
		info->gr += 1;
		info->gr_passed = TRUE;
	}
	info->stack_size += 4;
}

static void
add_int64_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr+1 > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		g_assert (info->stack_size % 4 == 0);
		info->stack_size += (info->stack_size % 8);

		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
	}
	else {
		// info->gr must be a0 or a2
		info->gr += (info->gr - MIPS_FIRST_ARG_REG) % 2;
		g_assert(info->gr <= MIPS_LAST_ARG_REG);

		ainfo->regtype = RegTypeGeneral;
		ainfo->reg = info->gr;
		info->gr += 2;
		info->gr_passed = TRUE;
	}
	info->stack_size += 8;
}

static void
add_float32_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
	}
	else {
		/* Only use FP regs for args if no int args passed yet */
		if (!info->gr_passed && info->fr <= MIPS_LAST_FPARG_REG) {
			ainfo->regtype = RegTypeFP;
			ainfo->reg = info->fr;
			/* Even though it's a single-precision float, it takes up two FP regs */
			info->fr += 2;
			/* FP and GP slots do not overlap */
			info->gr += 1;
		}
		else {
			/* Passing single-precision float arg in a GP register
			 * such as: func (0, 1.0, 2, 3);
			 * In this case, only one 'gr' register is consumed.
			 */
			ainfo->regtype = RegTypeGeneral;
			ainfo->reg = info->gr;

			info->gr += 1;
			info->gr_passed = TRUE;
		}
	}
	info->stack_size += 4;
}

static void
add_float64_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr+1 > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		g_assert(info->stack_size % 4 == 0);
		info->stack_size += (info->stack_size % 8);

		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
	}
	else {
		/* Only use FP regs for args if no int args passed yet */
		if (!info->gr_passed && info->fr <= MIPS_LAST_FPARG_REG) {
			ainfo->regtype = RegTypeFP;
			ainfo->reg = info->fr;
			info->fr += 2;
			/* FP and GP slots do not overlap */
			info->gr += 2;
		}
		else {
			// info->gr must be a0 or a2
			info->gr += (info->gr - MIPS_FIRST_ARG_REG) % 2;
			g_assert(info->gr <= MIPS_LAST_ARG_REG);

			ainfo->regtype = RegTypeGeneral;
			ainfo->reg = info->gr;
			info->gr += 2;
			info->gr_passed = TRUE;
		}
	}
	info->stack_size += 8;
}
#elif _MIPS_SIM == _ABIN32
/*
 * N32 calling convention version
 */

static void
add_int32_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
		info->stack_size += SIZEOF_REGISTER;
	}
	else {
		ainfo->regtype = RegTypeGeneral;
		ainfo->reg = info->gr;
		info->gr += 1;
		info->gr_passed = TRUE;
	}
}

static void
add_int64_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack && info->gr > MIPS_LAST_ARG_REG)
		args_onto_stack (info);

	/* Now, place the argument */
	if (info->on_stack) {
		g_assert (info->stack_size % 4 == 0);
		info->stack_size += (info->stack_size % 8);

		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
		info->stack_size += SIZEOF_REGISTER;
	}
	else {
		g_assert (info->gr <= MIPS_LAST_ARG_REG);

		ainfo->regtype = RegTypeGeneral;
		ainfo->reg = info->gr;
		info->gr += 1;
		info->gr_passed = TRUE;
	}
}

static void
add_float32_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack) {
		if (info->gr > MIPS_LAST_ARG_REG)
			args_onto_stack (info);
		else if (info->fr > MIPS_LAST_FPARG_REG)
			args_onto_stack (info);
	}

	/* Now, place the argument */
	if (info->on_stack) {
		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
		info->stack_size += FREG_SIZE;
	}
	else {
		ainfo->regtype = RegTypeFP;
		ainfo->reg = info->fr;
		info->fr += 1;
		/* FP and GP slots do not overlap */
		info->gr += 1;
	}
}

static void
add_float64_arg (CallInfo *info, ArgInfo *ainfo) {
	/* First, see if we need to drop onto the stack */
	if (!info->on_stack) {
		if (info->gr > MIPS_LAST_ARG_REG)
			args_onto_stack (info);
		else if (info->fr > MIPS_LAST_FPARG_REG)
			args_onto_stack (info);
	}

	/* Now, place the argument */
	if (info->on_stack) {
		g_assert(info->stack_size % 4 == 0);
		info->stack_size += (info->stack_size % 8);

		ainfo->regtype = RegTypeBase;
		ainfo->reg = mips_sp; /* in the caller */
		ainfo->offset = info->stack_size;
		info->stack_size += FREG_SIZE;
	}
	else {
		ainfo->regtype = RegTypeFP;
		ainfo->reg = info->fr;
		info->fr += 1;
		/* FP and GP slots do not overlap */
		info->gr += 1;
	}
}
#endif

static CallInfo*
calculate_sizes (MonoMethodSignature *sig, gboolean is_pinvoke)
{
	guint i;
	int n = sig->hasthis + sig->param_count;
	guint32 simpletype;
	CallInfo *cinfo = g_malloc0 (sizeof (CallInfo) + sizeof (ArgInfo) * n);

	cinfo->fr = MIPS_FIRST_FPARG_REG;
	cinfo->gr = MIPS_FIRST_ARG_REG;
	cinfo->stack_size = 0;

	DEBUG(printf("calculate_sizes\n"));

	/* handle returning a struct */
	if (MONO_TYPE_ISSTRUCT (sig->ret)) {
		cinfo->struct_ret = cinfo->gr;
		add_int32_arg (cinfo, &cinfo->ret);
	}

	n = 0;
	if (sig->hasthis) {
		add_int32_arg (cinfo, cinfo->args + n);
		n++;
	}
        DEBUG(printf("params: %d\n", sig->param_count));
	for (i = 0; i < sig->param_count; ++i) {
		if ((sig->call_convention == MONO_CALL_VARARG) && (i == sig->sentinelpos)) {
                        /* Prevent implicit arguments and sig_cookie from
			   being passed in registers */
			args_onto_stack (cinfo);
                        /* Emit the signature cookie just before the implicit arguments */
			add_int32_arg (cinfo, &cinfo->sig_cookie);
                }
                DEBUG(printf("param %d: ", i));
		if (sig->params [i]->byref) {
			DEBUG(printf("byref\n"));
			add_int32_arg (cinfo, &cinfo->args[n]);
			n++;
			continue;
		}
		simpletype = mono_type_get_underlying_type (sig->params [i])->type;
		switch (simpletype) {
		case MONO_TYPE_BOOLEAN:
		case MONO_TYPE_I1:
		case MONO_TYPE_U1:
                        DEBUG(printf("1 byte\n"));
			cinfo->args [n].size = 1;
			add_int32_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_CHAR:
		case MONO_TYPE_I2:
		case MONO_TYPE_U2:
                        DEBUG(printf("2 bytes\n"));
			cinfo->args [n].size = 2;
			add_int32_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_I4:
		case MONO_TYPE_U4:
                        DEBUG(printf("4 bytes\n"));
			cinfo->args [n].size = 4;
			add_int32_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_I:
		case MONO_TYPE_U:
		case MONO_TYPE_PTR:
		case MONO_TYPE_FNPTR:
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
		case MONO_TYPE_STRING:
		case MONO_TYPE_SZARRAY:
		case MONO_TYPE_ARRAY:
			cinfo->args [n].size = sizeof (gpointer);
			add_int32_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_GENERICINST:
			if (!mono_type_generic_inst_is_valuetype (sig->params [i])) {
				cinfo->args [n].size = sizeof (gpointer);
				add_int32_arg (cinfo, &cinfo->args[n]);
				n++;
				break;
			}
			/* Fall through */
		case MONO_TYPE_VALUETYPE: {
			int j;
			int nwords = 0;
			int has_offset = FALSE;
			ArgInfo dummy_arg;
			gint size, alignment;
			MonoClass *klass;

			klass = mono_class_from_mono_type (sig->params [i]);
			if (is_pinvoke)
			    size = mono_class_native_size (klass, NULL);
			else
			    size = mono_class_value_size (klass, NULL);
			alignment = mono_class_min_align (klass);
#if MIPS_PASS_STRUCTS_BY_VALUE
			/* Need to do alignment if struct contains long or double */
			if (alignment > 4) {
				if (cinfo->stack_size & (alignment - 1)) {
					add_int32_arg (cinfo, &dummy_arg);
				}
				g_assert (!(cinfo->stack_size & (alignment - 1)));
			}

#if 0
			g_printf ("valuetype struct size=%d offset=%d align=%d\n",
				  mono_class_native_size (sig->params [i]->data.klass, NULL),
				  cinfo->stack_size, alignment);
#endif
			nwords = (size + sizeof (gpointer) -1 ) / sizeof (gpointer);
			g_assert (cinfo->args [n].size == 0);
			g_assert (cinfo->args [n].vtsize == 0);
			for (j = 0; j < nwords; ++j) {
				if (j == 0) {
					add_int32_arg (cinfo, &cinfo->args [n]);
					if (cinfo->on_stack)
						has_offset = TRUE;
				} else {
					add_int32_arg (cinfo, &dummy_arg);
					if (!has_offset && cinfo->on_stack) {
						cinfo->args [n].offset = dummy_arg.offset;
						has_offset = TRUE;
					}
				}
				if (cinfo->on_stack)
					cinfo->args [n].vtsize += 1;
				else
					cinfo->args [n].size += 1;
			}
			//g_printf ("\tstack_size=%d vtsize=%d\n", cinfo->args [n].size, cinfo->args[n].vtsize);
			cinfo->args [n].regtype = RegTypeStructByVal;
#else
			add_int32_arg (cinfo, &cinfo->args[n]);
			cinfo->args [n].regtype = RegTypeStructByAddr;
#endif
			n++;
			break;
		}
		case MONO_TYPE_TYPEDBYREF: {
			/* keep in sync or merge with the valuetype case */
#if MIPS_PASS_STRUCTS_BY_VALUE
			{
				int size = sizeof (MonoTypedRef);
				int nwords = (size + sizeof (gpointer) -1 ) / sizeof (gpointer);
				cinfo->args [n].regtype = RegTypeStructByVal;
				if (!cinfo->on_stack && cinfo->gr <= MIPS_LAST_ARG_REG) {
					int rest = MIPS_LAST_ARG_REG - cinfo->gr + 1;
					int n_in_regs = rest >= nwords? nwords: rest;
					cinfo->args [n].size = n_in_regs;
					cinfo->args [n].vtsize = nwords - n_in_regs;
					cinfo->args [n].reg = cinfo->gr;
					cinfo->gr += n_in_regs;
					cinfo->gr_passed = TRUE;
				} else {
					cinfo->args [n].size = 0;
					cinfo->args [n].vtsize = nwords;
				}
				if (cinfo->args [n].vtsize > 0) {
					if (!cinfo->on_stack)
						args_onto_stack (cinfo);
					g_assert(cinfo->on_stack);
					cinfo->args [n].offset = cinfo->stack_size;
					g_print ("offset for arg %d at %d\n", n, cinfo->args [n].offset);
					cinfo->stack_size += cinfo->args [n].vtsize * sizeof (gpointer);
				}
			}
#else
			add_int32_arg (cinfo, &cinfo->args[n]);
			cinfo->args [n].regtype = RegTypeStructByAddr;
#endif
			n++;
			break;
		}
		case MONO_TYPE_U8:
		case MONO_TYPE_I8:
                        DEBUG(printf("8 bytes\n"));
			cinfo->args [n].size = 8;
			add_int64_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_R4:
                        DEBUG(printf("R4\n"));
			cinfo->args [n].size = 4;
			add_float32_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		case MONO_TYPE_R8:
                        DEBUG(printf("R8\n"));
			cinfo->args [n].size = 8;
			add_float64_arg (cinfo, &cinfo->args[n]);
			n++;
			break;
		default:
			g_error ("Can't trampoline 0x%x", sig->params [i]->type);
		}
	}

	{
		simpletype = mono_type_get_underlying_type (sig->ret)->type;
		switch (simpletype) {
		case MONO_TYPE_BOOLEAN:
		case MONO_TYPE_I1:
		case MONO_TYPE_U1:
		case MONO_TYPE_I2:
		case MONO_TYPE_U2:
		case MONO_TYPE_CHAR:
		case MONO_TYPE_I4:
		case MONO_TYPE_U4:
		case MONO_TYPE_I:
		case MONO_TYPE_U:
		case MONO_TYPE_PTR:
		case MONO_TYPE_FNPTR:
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
		case MONO_TYPE_SZARRAY:
		case MONO_TYPE_ARRAY:
		case MONO_TYPE_STRING:
			cinfo->ret.reg = mips_v0;
			break;
		case MONO_TYPE_U8:
		case MONO_TYPE_I8:
			cinfo->ret.reg = mips_v0;
			break;
		case MONO_TYPE_R4:
		case MONO_TYPE_R8:
			cinfo->ret.reg = mips_f0;
			cinfo->ret.regtype = RegTypeFP;
			break;
		case MONO_TYPE_GENERICINST:
			if (!mono_type_generic_inst_is_valuetype (sig->ret)) {
				cinfo->ret.reg = mips_v0;
				break;
			}
			break;
		case MONO_TYPE_VALUETYPE:
			break;
		case MONO_TYPE_TYPEDBYREF:
		case MONO_TYPE_VOID:
			break;
		default:
			g_error ("Can't handle as return value 0x%x", sig->ret->type);
		}
	}

	/* align stack size to 16 */
	cinfo->stack_size = (cinfo->stack_size + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);

	cinfo->stack_usage = cinfo->stack_size;
	return cinfo;
}


/*
 * Set var information according to the calling convention. mips version.
 * The locals var stuff should most likely be split in another method.
 */
void
mono_arch_allocate_vars (MonoCompile *cfg)
{
	MonoMethodSignature *sig;
	MonoMethodHeader *header;
	MonoInst *inst;
	int i, offset, size, align, curinst;
	int frame_reg = mips_sp;
	guint32 iregs_to_save = 0;
#if SAVE_FP_REGS
	guint32 fregs_to_restore;
#endif

	/* spill down, we'll fix it in a separate pass */
	// cfg->flags |= MONO_CFG_HAS_SPILLUP;

	/* allow room for the vararg method args: void* and long/double */
	if (mono_jit_trace_calls != NULL && mono_trace_eval (cfg->method))
		cfg->param_area = MAX (cfg->param_area, sizeof (gpointer)*8);

	/* this is bug #60332: remove when #59509 is fixed, so no weird vararg 
	 * call convs needs to be handled this way.
	 */
	if (cfg->flags & MONO_CFG_HAS_VARARGS)
		cfg->param_area = MAX (cfg->param_area, sizeof (gpointer)*8);

	/* gtk-sharp and other broken code will dllimport vararg functions even with
	 * non-varargs signatures. Since there is little hope people will get this right
	 * we assume they won't.
	 */
	if (cfg->method->wrapper_type == MONO_WRAPPER_MANAGED_TO_NATIVE)
		cfg->param_area = MAX (cfg->param_area, sizeof (gpointer)*8);

	/* a0-a3 always present */
	cfg->param_area = MAX (cfg->param_area, MIPS_STACK_PARAM_OFFSET);

	header = mono_method_get_header (cfg->method);

	sig = mono_method_signature (cfg->method);
	
	/* 
	 * We use the frame register also for any method that has
	 * exception clauses. This way, when the handlers are called,
	 * the code will reference local variables using the frame reg instead of
	 * the stack pointer: if we had to restore the stack pointer, we'd
	 * corrupt the method frames that are already on the stack (since
	 * filters get called before stack unwinding happens) when the filter
	 * code would call any method (this also applies to finally etc.).
	 */ 

	if ((cfg->flags & MONO_CFG_HAS_ALLOCA) || header->num_clauses || ALWAYS_USE_FP)
		frame_reg = mips_fp;
	cfg->frame_reg = frame_reg;
	if (frame_reg != mips_sp) {
		cfg->used_int_regs |= 1 << frame_reg;
	}

	offset = 0;
	curinst = 0;
	if (!MONO_TYPE_ISSTRUCT (sig->ret)) {
		/* FIXME: handle long and FP values */
		switch (mono_type_get_underlying_type (sig->ret)->type) {
		case MONO_TYPE_VOID:
			break;
		case MONO_TYPE_R4:
		case MONO_TYPE_R8:
			cfg->ret->opcode = OP_REGVAR;
			cfg->ret->inst_c0 = cfg->ret->dreg = mips_f0;
			break;
		default:
			cfg->ret->opcode = OP_REGVAR;
			cfg->ret->inst_c0 = mips_v0;
			break;
		}
	}
	/* Space for outgoing parameters, including a0-a3 */
	offset += cfg->param_area;

	/* allow room to save the return value (if it's a struct) */
	if (mono_jit_trace_calls != NULL && mono_trace_eval (cfg->method))
		offset += 8;

	if (sig->call_convention == MONO_CALL_VARARG) {
                cfg->sig_cookie = MIPS_STACK_PARAM_OFFSET;
        }

	/* Now handle the local variables */

	curinst = cfg->locals_start;
	for (i = curinst; i < cfg->num_varinfo; ++i) {
		inst = cfg->varinfo [i];
		if ((inst->flags & MONO_INST_IS_DEAD) || inst->opcode == OP_REGVAR)
			continue;

		/* inst->backend.is_pinvoke indicates native sized value types, this is used by the
		 * pinvoke wrappers when they call functions returning structure
		 */
		if (inst->backend.is_pinvoke && MONO_TYPE_ISSTRUCT (inst->inst_vtype) && inst->inst_vtype->type != MONO_TYPE_TYPEDBYREF)
			size = mono_class_native_size (mono_class_from_mono_type (inst->inst_vtype), &align);
		else
			size = mono_type_size (inst->inst_vtype, &align);

		offset += align - 1;
		offset &= ~(align - 1);
		inst->inst_offset = offset;
		inst->opcode = OP_REGOFFSET;
		inst->inst_basereg = frame_reg;
		offset += size;
		// g_print ("allocating local %d to %d\n", i, inst->inst_offset);
	}

	/* Space for LMF (if needed) */
#if SAVE_LMF
	if (cfg->method->save_lmf) {
		/* align the offset to 16 bytes */
		offset = (offset + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);
		cfg->arch.lmf_offset = offset;
		offset += sizeof (MonoLMF);
	}
#endif

	/* XXX - Saved S-regs seem to be getting clobbered by some calls with struct
	 * args or return vals.  Extra stack space avoids this in a lot of cases.
	 */
	offset += EXTRA_STACK_SPACE;

	/* Space for saved registers */
	cfg->arch.iregs_offset = offset;
#if SAVE_ALL_REGS
	iregs_to_save = MONO_ARCH_CALLEE_SAVED_REGS;
#else
	iregs_to_save = (cfg->used_int_regs & MONO_ARCH_CALLEE_SAVED_REGS);
#endif
	if (iregs_to_save) {
		for (i = MONO_MAX_IREGS-1; i >= 0; --i) {
			if (iregs_to_save & (1 << i)) {
				offset += SIZEOF_REGISTER;
			}
		}
	}

	/* XXX - Saved S-regs seem to be getting clobbered by some calls with struct
	 * args or return vals.  Extra stack space avoids this in a lot of cases.
	 */
	offset += EXTRA_STACK_SPACE;

	/* saved float registers */
#if SAVE_FP_REGS
	fregs_to_restore = (cfg->used_float_regs & MONO_ARCH_CALLEE_SAVED_FREGS);
	if (fregs_to_restore) {
		for (i = MONO_MAX_FREGS-1; i >= 0; --i) {
			if (fregs_to_restore & (1 << i)) {
				offset += sizeof(double);
			}
		}
	}
#endif

#if _MIPS_SIM == _ABIO32
	/* Now add space for saving the ra */
	offset += SIZEOF_VOID_P;

	/* change sign? */
	offset = (offset + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);
	cfg->stack_offset = offset;
	cfg->arch.local_alloc_offset = cfg->stack_offset;
#endif

	/*
	 * Now allocate stack slots for the int arg regs (a0 - a3)
	 * On MIPS o32, these are just above the incoming stack pointer
	 * Even if the arg has been assigned to a regvar, it gets a stack slot
	 */

	/* Return struct-by-value results in a hidden first argument */
	if (MONO_TYPE_ISSTRUCT (sig->ret)) {
		cfg->vret_addr->opcode = OP_REGOFFSET;
		cfg->vret_addr->inst_c0 = mips_a0;
		cfg->vret_addr->inst_offset = offset;
		cfg->vret_addr->inst_basereg = frame_reg;
		offset += SIZEOF_REGISTER;
	}

	for (i = 0; i < sig->param_count + sig->hasthis; ++i) {
		inst = cfg->args [i];
		if (inst->opcode != OP_REGVAR) {
			MonoType *arg_type;
		 
			if (sig->hasthis && (i == 0))
				arg_type = &mono_defaults.object_class->byval_arg;
			else
				arg_type = sig->params [i - sig->hasthis];

			inst->opcode = OP_REGOFFSET;
			size = mono_type_size (arg_type, &align);

			if (size < SIZEOF_REGISTER) {
				size = SIZEOF_REGISTER;
				align = SIZEOF_REGISTER;
			}
			inst->inst_basereg = frame_reg;
			offset = (offset + align - 1) & ~(align - 1);
			inst->inst_offset = offset;
			offset += size;
			if ((sig->call_convention == MONO_CALL_VARARG) && (i < sig->sentinelpos)) 
				cfg->sig_cookie += size;
			// g_print ("allocating param %d to %d\n", i, inst->inst_offset);
		}
		else {
#if _MIPS_SIM == _ABIO32
			/* o32: Even a0-a3 get stack slots */
			size = SIZEOF_REGISTER;
			align = SIZEOF_REGISTER;
			inst->inst_basereg = frame_reg;
			offset = (offset + align - 1) & ~(align - 1);
			inst->inst_offset = offset;
			offset += size;
			if ((sig->call_convention == MONO_CALL_VARARG) && (i < sig->sentinelpos)) 
				cfg->sig_cookie += size;
			// g_print ("allocating param %d to %d\n", i, inst->inst_offset);
#endif
		}
	}
#if _MIPS_SIM == _ABIN32
	/* Now add space for saving the ra */
	offset += SIZEOF_VOID_P;

	/* change sign? */
	offset = (offset + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);
	cfg->stack_offset = offset;
	cfg->arch.local_alloc_offset = cfg->stack_offset;
#endif
}

void
mono_arch_create_vars (MonoCompile *cfg)
{
	MonoMethodSignature *sig;

	sig = mono_method_signature (cfg->method);

	if (MONO_TYPE_ISSTRUCT (sig->ret)) {
		cfg->vret_addr = mono_compile_create_var (cfg, &mono_defaults.int_class->byval_arg, OP_ARG);
		if (G_UNLIKELY (cfg->verbose_level > 1)) {
			printf ("vret_addr = ");
			mono_print_ins (cfg->vret_addr);
		}
	}
}

/* Fixme: we need an alignment solution for enter_method and mono_arch_call_opcode,
 * currently alignment in mono_arch_call_opcode is computed without arch_get_argument_info 
 */

/* 
 * take the arguments and generate the arch-specific
 * instructions to properly call the function in call.
 * This includes pushing, moving arguments to the right register
 * etc.
 * Issue: who does the spilling if needed, and when?
 */
static void
emit_sig_cookie (MonoCompile *cfg, MonoCallInst *call, CallInfo *cinfo)
{
	int sig_reg = mono_alloc_ireg (cfg);

	MONO_EMIT_NEW_ICONST (cfg, sig_reg, (guint32)call->signature);
	MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STORE_MEMBASE_REG,
			mips_sp, cinfo->sig_cookie.offset, sig_reg);
}

void
mono_arch_emit_call (MonoCompile *cfg, MonoCallInst *call)
{
	MonoInst *in, *ins;
	MonoMethodSignature *sig;
	int i, n;
	CallInfo *cinfo;
	int is_virtual = 0;

	sig = call->signature;
	n = sig->param_count + sig->hasthis;
	
	cinfo = calculate_sizes (sig, sig->pinvoke);
	if (cinfo->struct_ret)
		call->used_iregs |= 1 << cinfo->struct_ret;

	for (i = 0; i < n; ++i) {
		ArgInfo *ainfo = cinfo->args + i;
		MonoType *t;

		if (i >= sig->hasthis)
			t = sig->params [i - sig->hasthis];
		else
			t = &mono_defaults.int_class->byval_arg;
		t = mini_type_get_underlying_type (cfg->generic_sharing_context, t);

		if (!sig->pinvoke && (sig->call_convention == MONO_CALL_VARARG) && (i == sig->sentinelpos))
			emit_sig_cookie (cfg, call, cinfo);
		if (is_virtual && i == 0) {
			/* the argument will be attached to the call instrucion */
			in = call->args [i];
			call->used_iregs |= 1 << ainfo->reg;
			continue;
		}
		in = call->args [i];
		if (ainfo->regtype == RegTypeGeneral) {
#if SIZEOF_REGISTER == 4
			if (!t->byref && ((t->type == MONO_TYPE_I8) || (t->type == MONO_TYPE_U8))) {
				MONO_INST_NEW (cfg, ins, OP_MOVE);
				ins->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = in->dreg + 1;
				MONO_ADD_INS (cfg->cbb, ins);
				mono_call_inst_add_outarg_reg (cfg, call, ins->dreg, ainfo->reg + 1, FALSE);

				MONO_INST_NEW (cfg, ins, OP_MOVE);
				ins->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = in->dreg + 2;
				MONO_ADD_INS (cfg->cbb, ins);
				mono_call_inst_add_outarg_reg (cfg, call, ins->dreg, ainfo->reg, FALSE);
			} else
#endif
			if (!t->byref && (t->type == MONO_TYPE_R4)) {
				int freg;

#if PROMOTE_R4_TO_R8
				/* ??? - convert to single first? */
				MONO_INST_NEW (cfg, ins, OP_MIPS_CVTSD);
				ins->dreg = mono_alloc_freg (cfg);
				ins->sreg1 = in->dreg;
				MONO_ADD_INS (cfg->cbb, ins);
				freg = ins->dreg;
#else
				freg = in->dreg;
#endif
				/* trying to load float value into int registers */
				MONO_INST_NEW (cfg, ins, OP_MIPS_MFC1S);
				ins->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = freg;
				MONO_ADD_INS (cfg->cbb, ins);
				mono_call_inst_add_outarg_reg (cfg, call, ins->dreg, ainfo->reg, FALSE);
			} else if (!t->byref && (t->type == MONO_TYPE_R8)) {
				/* trying to load float value into int registers */
				MONO_INST_NEW (cfg, ins, OP_MIPS_MFC1D);
				ins->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = in->dreg;
				MONO_ADD_INS (cfg->cbb, ins);
				mono_call_inst_add_outarg_reg (cfg, call, ins->dreg, ainfo->reg, FALSE);
			} else {
				MONO_INST_NEW (cfg, ins, OP_MOVE);
				ins->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = in->dreg;
				MONO_ADD_INS (cfg->cbb, ins);
				mono_call_inst_add_outarg_reg (cfg, call, ins->dreg, ainfo->reg, FALSE);
			}
		} else if (ainfo->regtype == RegTypeStructByAddr) {
			MONO_INST_NEW (cfg, ins, OP_OUTARG_VT);
			ins->opcode = OP_OUTARG_VT;
			ins->sreg1 = in->dreg;
			ins->klass = in->klass;
			ins->inst_p0 = call;
			ins->inst_p1 = mono_mempool_alloc (cfg->mempool, sizeof (ArgInfo));
			memcpy (ins->inst_p1, ainfo, sizeof (ArgInfo));
			MONO_ADD_INS (cfg->cbb, ins);
		} else if (ainfo->regtype == RegTypeStructByVal) {
			/* this is further handled in mono_arch_emit_outarg_vt () */
			MONO_INST_NEW (cfg, ins, OP_OUTARG_VT);
			ins->opcode = OP_OUTARG_VT;
			ins->sreg1 = in->dreg;
			ins->klass = in->klass;
			ins->inst_p0 = call;
			ins->inst_p1 = mono_mempool_alloc (cfg->mempool, sizeof (ArgInfo));
			memcpy (ins->inst_p1, ainfo, sizeof (ArgInfo));
			MONO_ADD_INS (cfg->cbb, ins);
		} else if (ainfo->regtype == RegTypeBase) {
			if (!t->byref && ((t->type == MONO_TYPE_I8) || (t->type == MONO_TYPE_U8))) {
				MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STOREI8_MEMBASE_REG, mips_sp, ainfo->offset, in->dreg);
			} else if (!t->byref && ((t->type == MONO_TYPE_R4) || (t->type == MONO_TYPE_R8))) {
				if (t->type == MONO_TYPE_R8)
					MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STORER8_MEMBASE_REG, mips_sp, ainfo->offset, in->dreg);
				else
					MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STORER4_MEMBASE_REG, mips_sp, ainfo->offset, in->dreg);
			} else {
				MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STORE_MEMBASE_REG, mips_sp, ainfo->offset, in->dreg);
			}
		} else if (ainfo->regtype == RegTypeFP) {
			if (t->type == MONO_TYPE_VALUETYPE) {
				/* this is further handled in mono_arch_emit_outarg_vt () */
				MONO_INST_NEW (cfg, ins, OP_OUTARG_VT);
				ins->opcode = OP_OUTARG_VT;
				ins->sreg1 = in->dreg;
				ins->klass = in->klass;
				ins->inst_p0 = call;
				ins->inst_p1 = mono_mempool_alloc (cfg->mempool, sizeof (ArgInfo));
				memcpy (ins->inst_p1, ainfo, sizeof (ArgInfo));
				MONO_ADD_INS (cfg->cbb, ins);

				cfg->flags |= MONO_CFG_HAS_FPOUT;
			} else {
				int dreg = mono_alloc_freg (cfg);

				if (ainfo->size == 4) {
					MONO_EMIT_NEW_UNALU (cfg, OP_MIPS_CVTSD, dreg, in->dreg);
				} else {
					MONO_INST_NEW (cfg, ins, OP_FMOVE);
					ins->dreg = dreg;
					ins->sreg1 = in->dreg;
					MONO_ADD_INS (cfg->cbb, ins);
				}

				mono_call_inst_add_outarg_reg (cfg, call, dreg, ainfo->reg, TRUE);
				cfg->flags |= MONO_CFG_HAS_FPOUT;
			}
		} else {
			g_assert_not_reached ();
		}
	}

	/* Emit the signature cookie in the case that there is no
	   additional argument */
	if (!sig->pinvoke && (sig->call_convention == MONO_CALL_VARARG) && (n == sig->sentinelpos))
		emit_sig_cookie (cfg, call, cinfo);

	if (cinfo->struct_ret) {
		MonoInst *vtarg;

		MONO_INST_NEW (cfg, vtarg, OP_MOVE);
		vtarg->sreg1 = call->vret_var->dreg;
		vtarg->dreg = mono_alloc_preg (cfg);
		MONO_ADD_INS (cfg->cbb, vtarg);

		mono_call_inst_add_outarg_reg (cfg, call, vtarg->dreg, cinfo->struct_ret, FALSE);
	}
#if 0
	/*
	 * Reverse the call->out_args list.
	 */
	{
		MonoInst *prev = NULL, *list = call->out_args, *next;
		while (list) {
			next = list->next;
			list->next = prev;
			prev = list;
			list = next;
		}
		call->out_args = prev;
	}
#endif
	call->stack_usage = cinfo->stack_usage;
	cfg->param_area = MAX (cfg->param_area, cinfo->stack_usage);
#if _MIPS_SIM == _ABIO32
	/* a0-a3 always present */
	cfg->param_area = MAX (cfg->param_area, 4 * SIZEOF_REGISTER);
#endif
	cfg->param_area = (cfg->param_area + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);
	cfg->flags |= MONO_CFG_HAS_CALLS;
	/* 
	 * should set more info in call, such as the stack space
	 * used by the args that needs to be added back to esp
	 */

	g_free (cinfo);
}

void
mono_arch_emit_outarg_vt (MonoCompile *cfg, MonoInst *ins, MonoInst *src)
{
	MonoCallInst *call = (MonoCallInst*)ins->inst_p0;
	ArgInfo *ainfo = ins->inst_p1;
	int ovf_size = ainfo->vtsize;
	int doffset = ainfo->offset;
	int i, soffset, dreg;

	if (ainfo->regtype == RegTypeStructByVal) {
#if 1
		if (cfg->verbose_level > 0) {
			char* nm = mono_method_full_name (cfg->method, TRUE);
			g_print ("Method %s outarg_vt struct doffset=%d ainfo->size=%d ovf_size=%d\n", 
				 nm, doffset, ainfo->size, ovf_size);
			g_free (nm);
		}
#endif

		soffset = 0;
		for (i = 0; i < ainfo->size; ++i) {
			dreg = mono_alloc_ireg (cfg);
			MONO_EMIT_NEW_LOAD_MEMBASE (cfg, dreg, src->dreg, soffset);
			mono_call_inst_add_outarg_reg (cfg, call, dreg, ainfo->reg + i, FALSE);
			soffset += SIZEOF_REGISTER;
		}
		if (ovf_size != 0) {
			mini_emit_memcpy (cfg, mips_fp, doffset, src->dreg, soffset, ovf_size * sizeof (gpointer), 0);
		}
	} else if (ainfo->regtype == RegTypeFP) {
		int tmpr = mono_alloc_freg (cfg);

		if (ainfo->size == 4)
			MONO_EMIT_NEW_LOAD_MEMBASE_OP (cfg, OP_LOADR4_MEMBASE, tmpr, src->dreg, 0);
		else
			MONO_EMIT_NEW_LOAD_MEMBASE_OP (cfg, OP_LOADR8_MEMBASE, tmpr, src->dreg, 0);
		dreg = mono_alloc_freg (cfg);
		MONO_EMIT_NEW_UNALU (cfg, OP_FMOVE, dreg, tmpr);
		mono_call_inst_add_outarg_reg (cfg, call, dreg, ainfo->reg, TRUE);
	} else {
		MonoInst *vtcopy = mono_compile_create_var (cfg, &src->klass->byval_arg, OP_LOCAL);
		MonoInst *load;
		guint32 size;

		/* FIXME: alignment? */
		if (call->signature->pinvoke) {
			size = mono_type_native_stack_size (&src->klass->byval_arg, NULL);
			vtcopy->backend.is_pinvoke = 1;
		} else {
			size = mini_type_stack_size (cfg->generic_sharing_context, &src->klass->byval_arg, NULL);
		}
		if (size > 0)
			g_assert (ovf_size > 0);

		EMIT_NEW_VARLOADA (cfg, load, vtcopy, vtcopy->inst_vtype);
		mini_emit_memcpy (cfg, load->dreg, 0, src->dreg, 0, size, 0);

		if (ainfo->offset)
			MONO_EMIT_NEW_STORE_MEMBASE (cfg, OP_STORE_MEMBASE_REG, mips_at, ainfo->offset, load->dreg);
		else
			mono_call_inst_add_outarg_reg (cfg, call, load->dreg, ainfo->reg, FALSE);
	}
}

void
mono_arch_emit_setret (MonoCompile *cfg, MonoMethod *method, MonoInst *val)
{
	MonoType *ret = mini_type_get_underlying_type (cfg->generic_sharing_context,
			mono_method_signature (method)->ret);

	if (!ret->byref) {
#if (SIZEOF_REGISTER == 4)
		if (ret->type == MONO_TYPE_I8 || ret->type == MONO_TYPE_U8) {
			MonoInst *ins;

			MONO_INST_NEW (cfg, ins, OP_SETLRET);
			ins->sreg1 = val->dreg + 1;
			ins->sreg2 = val->dreg + 2;
			MONO_ADD_INS (cfg->cbb, ins);
			return;
		}
#endif
		if (ret->type == MONO_TYPE_R8) {
			MONO_EMIT_NEW_UNALU (cfg, OP_FMOVE, cfg->ret->dreg, val->dreg);
			return;
		}
		if (ret->type == MONO_TYPE_R4) {
			MONO_EMIT_NEW_UNALU (cfg, OP_MIPS_CVTSD, cfg->ret->dreg, val->dreg);
			return;
		}
	}
	MONO_EMIT_NEW_UNALU (cfg, OP_MOVE, cfg->ret->dreg, val->dreg);
}

void
mono_arch_peephole_pass_1 (MonoCompile *cfg, MonoBasicBlock *bb)
{
	MonoInst *ins, *n, *last_ins = NULL;

	if (cfg->verbose_level > 2)
		g_print ("Basic block %d peephole pass 1\n", bb->block_num);

	ins = bb->code;
	MONO_BB_FOR_EACH_INS_SAFE (bb, n, ins) {
		if (cfg->verbose_level > 2)
			mono_print_ins_index (0, ins);

		switch (ins->opcode) {
#if 0
		case OP_LOAD_MEMBASE:
		case OP_LOADI4_MEMBASE:
			/*
			 * OP_IADD		reg2, reg1, const1
			 * OP_LOAD_MEMBASE	const2(reg2), reg3
			 * ->
			 * OP_LOAD_MEMBASE	(const1+const2)(reg1), reg3
			 */
			if (last_ins && (last_ins->opcode == OP_IADD_IMM || last_ins->opcode == OP_ADD_IMM) && (last_ins->dreg == ins->inst_basereg) && (last_ins->sreg1 != last_ins->dreg)){
				int const1 = last_ins->inst_imm;
				int const2 = ins->inst_offset;

				if (mips_is_imm16 (const1 + const2)) {
					ins->inst_basereg = last_ins->sreg1;
					ins->inst_offset = const1 + const2;
				}
			}
			break;
#endif

		}
		last_ins = ins;
		ins = ins->next;
	}
	bb->last_ins = last_ins;
}

void
mono_arch_peephole_pass_2 (MonoCompile *cfg, MonoBasicBlock *bb)
{
	MonoInst *ins, *n, *last_ins = NULL;
	ins = bb->code;

	MONO_BB_FOR_EACH_INS_SAFE (bb, n, ins) {
		MonoInst *last_ins = ins->prev;

		switch (ins->opcode) {
		case OP_MUL_IMM: 
			/* remove unnecessary multiplication with 1 */
			if (ins->inst_imm == 1) {
				if (ins->dreg != ins->sreg1) {
					ins->opcode = OP_MOVE;
				} else {
					MONO_DELETE_INS (bb, ins);
					continue;
				}
			} else {
				int power2 = mono_is_power_of_two (ins->inst_imm);
				if (power2 > 0) {
					ins->opcode = OP_SHL_IMM;
					ins->inst_imm = power2;
				}
			}
			break;
		case OP_LOAD_MEMBASE:
		case OP_LOADI4_MEMBASE:
			/* 
			 * OP_STORE_MEMBASE_REG reg, offset(basereg) 
			 * OP_LOAD_MEMBASE offset(basereg), reg
			 */
			if (last_ins && (last_ins->opcode == OP_STOREI4_MEMBASE_REG 
					 || last_ins->opcode == OP_STORE_MEMBASE_REG) &&
			    ins->inst_basereg == last_ins->inst_destbasereg &&
			    ins->inst_offset == last_ins->inst_offset) {
				if (ins->dreg == last_ins->sreg1) {
					MONO_DELETE_INS (bb, ins);
					continue;
				} else {
					//static int c = 0; printf ("MATCHX %s %d\n", cfg->method->name,c++);
					ins->opcode = OP_MOVE;
					ins->sreg1 = last_ins->sreg1;
				}
				break;
			}
			/* 
			 * Note: reg1 must be different from the basereg in the second load
			 * OP_LOAD_MEMBASE offset(basereg), reg1
			 * OP_LOAD_MEMBASE offset(basereg), reg2
			 * -->
			 * OP_LOAD_MEMBASE offset(basereg), reg1
			 * OP_MOVE reg1, reg2
			 */
			if (last_ins && (last_ins->opcode == OP_LOADI4_MEMBASE
					   || last_ins->opcode == OP_LOAD_MEMBASE) &&
			      ins->inst_basereg != last_ins->dreg &&
			      ins->inst_basereg == last_ins->inst_basereg &&
			      ins->inst_offset == last_ins->inst_offset) {

				if (ins->dreg == last_ins->dreg) {
					MONO_DELETE_INS (bb, ins);
					continue;
				} else {
					ins->opcode = OP_MOVE;
					ins->sreg1 = last_ins->dreg;
				}

				//g_assert_not_reached ();
				break;
			}
#if 0
			/* 
			 * OP_STORE_MEMBASE_IMM imm, offset(basereg) 
			 * OP_LOAD_MEMBASE offset(basereg), reg
			 * -->
			 * OP_STORE_MEMBASE_IMM imm, offset(basereg) 
			 * OP_ICONST reg, imm
			 */
			if (last_ins && (last_ins->opcode == OP_STOREI4_MEMBASE_IMM
						|| last_ins->opcode == OP_STORE_MEMBASE_IMM) &&
				   ins->inst_basereg == last_ins->inst_destbasereg &&
				   ins->inst_offset == last_ins->inst_offset) {
				//static int c = 0; printf ("MATCHX %s %d\n", cfg->method->name,c++);
				ins->opcode = OP_ICONST;
				ins->inst_c0 = last_ins->inst_imm;
				g_assert_not_reached (); // check this rule
				break;
			}
#endif
			break;
		case OP_LOADU1_MEMBASE:
		case OP_LOADI1_MEMBASE:
			if (last_ins && (last_ins->opcode == OP_STOREI1_MEMBASE_REG) &&
					ins->inst_basereg == last_ins->inst_destbasereg &&
					ins->inst_offset == last_ins->inst_offset) {
				ins->opcode = (ins->opcode == OP_LOADI1_MEMBASE) ? OP_ICONV_TO_I1 : OP_ICONV_TO_U1;
				ins->sreg1 = last_ins->sreg1;				
			}
			break;
		case OP_LOADU2_MEMBASE:
		case OP_LOADI2_MEMBASE:
			if (last_ins && (last_ins->opcode == OP_STOREI2_MEMBASE_REG) &&
					ins->inst_basereg == last_ins->inst_destbasereg &&
					ins->inst_offset == last_ins->inst_offset) {
				ins->opcode = (ins->opcode == OP_LOADI2_MEMBASE) ? OP_ICONV_TO_I2 : OP_ICONV_TO_U2;
				ins->sreg1 = last_ins->sreg1;				
			}
			break;
		case OP_ICONV_TO_I4:
		case OP_ICONV_TO_U4:
		case OP_MOVE:
			ins->opcode = OP_MOVE;
			/* 
			 * OP_MOVE reg, reg 
			 */
			if (ins->dreg == ins->sreg1) {
				MONO_DELETE_INS (bb, ins);
				continue;
			}
			/* 
			 * OP_MOVE sreg, dreg 
			 * OP_MOVE dreg, sreg
			 */
			if (last_ins && last_ins->opcode == OP_MOVE &&
			    ins->sreg1 == last_ins->dreg &&
			    ins->dreg == last_ins->sreg1) {
				MONO_DELETE_INS (bb, ins);
				continue;
			}
			break;
		}
		last_ins = ins;
		ins = ins->next;
	}
	bb->last_ins = last_ins;
}

void
mono_arch_decompose_long_opts (MonoCompile *cfg, MonoInst *ins)
{
	int tmp1 = -1;
	int tmp2 = -1;
	int tmp3 = -1;
	int tmp4 = -1;
	int tmp5 = -1;

	switch (ins->opcode) {
#if 0
	case OP_LCOMPARE:
	case OP_LCOMPARE_IMM:
		mono_print_ins (ins);
		g_assert_not_reached ();
#endif
	case OP_LADD:
		tmp1 = mono_alloc_ireg (cfg);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->dreg+1, ins->sreg1+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, ins->dreg+2, tmp1);
		ins->opcode = OP_NOP;
		break;

	case OP_LADD_IMM:
		tmp1 = mono_alloc_ireg (cfg);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_IADD_IMM, ins->dreg+1, ins->sreg1+1, ins->inst_ls_word);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->dreg+1, ins->sreg1+1);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_IADD_IMM, ins->dreg+2, ins->sreg1+2, ins->inst_ms_word);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, ins->dreg+2, tmp1);
		ins->opcode = OP_NOP;
		break;

	case OP_LSUB:
		tmp1 = mono_alloc_ireg (cfg);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->sreg1+1, ins->dreg+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->dreg+2, tmp1);
		ins->opcode = OP_NOP;
		break;

	case OP_LSUB_IMM:
		tmp1 = mono_alloc_ireg (cfg);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_ISUB_IMM, ins->dreg+1, ins->sreg1+1, ins->inst_ls_word);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->sreg1+1, ins->dreg+1);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_ISUB_IMM, ins->dreg+2, ins->sreg1+2, ins->inst_ms_word);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->dreg+2, tmp1);
		ins->opcode = OP_NOP;
		break;

	case OP_LMUL:
	case OP_LDIV:
	case OP_LDIV_UN:
	case OP_LREM:
	case OP_LREM_UN:
	case OP_LSHL:
	case OP_LSHR:
	case OP_LSHR_UN:
		mono_print_ins (ins);
		g_assert_not_reached ();

	case OP_LNEG:
		tmp1 = mono_alloc_ireg (cfg);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+1, mips_zero, ins->sreg1+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, mips_zero, ins->dreg+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, mips_zero, ins->sreg1+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->dreg+2, tmp1);
		ins->opcode = OP_NOP;
		break;

#if 0
	case OP_LNOT:
#endif
#if 0
	case OP_LCONV_TO_I1:
	case OP_LCONV_TO_I2:
	case OP_LCONV_TO_I4:
	case OP_LCONV_TO_I8:
	case OP_LCONV_TO_R4:
	case OP_LCONV_TO_R8:
	case OP_LCONV_TO_U4:
	case OP_LCONV_TO_U8:
	case OP_LCONV_TO_U2:
	case OP_LCONV_TO_U1:
	case OP_LCONV_TO_I:
	case OP_LCONV_TO_OVF_I:
	case OP_LCONV_TO_OVF_U:
#endif
		mono_print_ins (ins);
		g_assert_not_reached ();

	case OP_LADD_OVF:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);
		tmp3 = mono_alloc_ireg (cfg);
		tmp4 = mono_alloc_ireg (cfg);
		tmp5 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);

		/* tmp1 holds the carry from the low 32-bit to the high 32-bits */
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp5, ins->dreg+1, ins->sreg1+1);

		/* add the high 32-bits, and add in the carry from the low 32-bits */
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, tmp5, ins->dreg+2);

		/* Overflow happens if
		 *	neg + neg = pos    or
		 *	pos + pos = neg
		 * XOR of the high bits returns 0 if the signs match
		 * XOR of that with the high bit of the result return 1 if overflow.
		 */

		/* tmp1 = 0 if the signs of the two inputs match, 1 otherwise */
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp1, ins->sreg1+2, ins->sreg2+2);

		/* set tmp2 = 0 if bit31 of results matches is different than the operands */
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp2, ins->dreg+2, ins->sreg2+2);
		MONO_EMIT_NEW_UNALU (cfg, OP_INOT, tmp2, tmp2);

		/* OR(tmp1, tmp2) = 0 if both conditions are true */
		MONO_EMIT_NEW_BIALU (cfg, OP_IOR, tmp3, tmp2, tmp1);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_SHR_IMM, tmp4, tmp3, 31);

		/* Now, if (tmp4 == 0) then overflow */
		MONO_EMIT_NEW_COMPARE_EXC (cfg, EQ, tmp4, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_LADD_OVF_UN:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->dreg+1, ins->sreg1+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg+2, tmp1, ins->dreg+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp2, ins->dreg+2, ins->sreg1+2);
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp2, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_LMUL_OVF:
	case OP_LMUL_OVF_UN:
		mono_print_ins (ins);
		g_assert_not_reached ();

	case OP_LSUB_OVF:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);
		tmp3 = mono_alloc_ireg (cfg);
		tmp4 = mono_alloc_ireg (cfg);
		tmp5 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);

		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp5, ins->sreg1+1, ins->dreg+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->dreg+2, tmp5);

		/* Overflow happens if
		 *	neg - pos = pos    or
		 *	pos - neg = neg
		 * XOR of bit31 of the lhs & rhs = 1 if the signs are different
		 *
		 * tmp1 = (lhs ^ rhs)
		 * tmp2 = (lhs ^ result)
		 * if ((tmp1 < 0) & (tmp2 < 0)) then overflow
		 */

		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp1, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp2, ins->sreg1+2, ins->dreg+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IAND, tmp3, tmp2, tmp1);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_SHR_IMM, tmp4, tmp3, 31);

		/* Now, if (tmp4 == 1) then overflow */
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp4, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_LSUB_OVF_UN:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+1, ins->sreg1+1, ins->sreg2+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->sreg1+1, ins->dreg+1);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->sreg1+2, ins->sreg2+2);
		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg+2, ins->dreg+2, tmp1);

		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp2, ins->sreg1+2, ins->dreg+2);
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp2, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;
#if 0
	case OP_LCONV_TO_OVF_I1_UN:
	case OP_LCONV_TO_OVF_I2_UN:
	case OP_LCONV_TO_OVF_I4_UN:
	case OP_LCONV_TO_OVF_I8_UN:
	case OP_LCONV_TO_OVF_U1_UN:
	case OP_LCONV_TO_OVF_U2_UN:
	case OP_LCONV_TO_OVF_U4_UN:
	case OP_LCONV_TO_OVF_U8_UN:
	case OP_LCONV_TO_OVF_I_UN:
	case OP_LCONV_TO_OVF_U_UN:
	case OP_LCONV_TO_OVF_I1:
	case OP_LCONV_TO_OVF_U1:
	case OP_LCONV_TO_OVF_I2:
	case OP_LCONV_TO_OVF_U2:
	case OP_LCONV_TO_OVF_I4:
	case OP_LCONV_TO_OVF_U4:
	case OP_LCONV_TO_OVF_I8:
	case OP_LCONV_TO_OVF_U8:
#endif
	case OP_LCEQ:
	case OP_LCGT:
	case OP_LCGT_UN:
	case OP_LCLT:
	case OP_LCLT_UN:
#if 0
	case OP_LCONV_TO_R_UN:
	case OP_LCONV_TO_U:
#endif
	case OP_LMUL_IMM:
	case OP_LSHL_IMM:
	case OP_LSHR_IMM:
	case OP_LSHR_UN_IMM:
	case OP_LDIV_IMM:
	case OP_LDIV_UN_IMM:
	case OP_LREM_IMM:
	case OP_LREM_UN_IMM:
	case OP_LBEQ:
	case OP_LBGE:
	case OP_LBGT:
	case OP_LBLE:
	case OP_LBLT:
	case OP_LBNE_UN:
	case OP_LBGE_UN:
	case OP_LBGT_UN:
	case OP_LBLE_UN:
	case OP_LBLT_UN:
		mono_print_ins (ins);
		g_assert_not_reached ();
#if 0
	case OP_LCONV_TO_R8_2:
	case OP_LCONV_TO_R4_2:
	case OP_LCONV_TO_R_UN_2:
#endif
	case OP_LCONV_TO_OVF_I4_2:
		tmp1 = mono_alloc_ireg (cfg);

		/* Overflows if reg2 != sign extension of reg1 */
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_SHR_IMM, tmp1, ins->sreg1, 31);
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, ins->sreg2, tmp1, "OverflowException");
		MONO_EMIT_NEW_UNALU (cfg, OP_MOVE, ins->dreg, ins->sreg1);
		ins->opcode = OP_NOP;
		break;

	case OP_LMIN_UN:
	case OP_LMAX_UN:
	case OP_LMIN:
	case OP_LMAX:
		mono_print_ins (ins);
		g_assert_not_reached ();

	default:
		break;
	}
}

void
mono_arch_decompose_opts (MonoCompile *cfg, MonoInst *ins)
{
	int tmp1 = -1;
	int tmp2 = -1;
	int tmp3 = -1;
	int tmp4 = -1;
	int tmp5 = -1;

	switch (ins->opcode) {
	case OP_IADD_OVF:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);
		tmp3 = mono_alloc_ireg (cfg);
		tmp4 = mono_alloc_ireg (cfg);
		tmp5 = mono_alloc_ireg (cfg);

		/* add the operands */

		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg, ins->sreg1, ins->sreg2);

		/* Overflow happens if
		 *	neg + neg = pos    or
		 *	pos + pos = neg
		 *
		 * (bit31s of operands match) AND (bit31 of operand != bit31 of result)
		 * XOR of the high bit returns 0 if the signs match
		 * XOR of that with the high bit of the result return 1 if overflow.
		 */

		/* tmp1 = 0 if the signs of the two inputs match, 1 otherwise */
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp1, ins->sreg1, ins->sreg2);

		/* set tmp2 = 0 if bit31 of results matches is different than the operands */
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp2, ins->dreg, ins->sreg2);
		MONO_EMIT_NEW_UNALU (cfg, OP_INOT, tmp3, tmp2);

		/* OR(tmp1, tmp2) = 0 if both conditions are true */
		MONO_EMIT_NEW_BIALU (cfg, OP_IOR, tmp4, tmp3, tmp1);

		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_SHR_IMM, tmp5, tmp4, 31);

		/* Now, if (tmp4 == 0) then overflow */
		MONO_EMIT_NEW_COMPARE_EXC (cfg, EQ, tmp5, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_IADD_OVF_UN:
		tmp1 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_IADD, ins->dreg, ins->sreg1, ins->sreg2);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->dreg, ins->sreg1);
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp1, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_ISUB_OVF:
		tmp1 = mono_alloc_ireg (cfg);
		tmp2 = mono_alloc_ireg (cfg);
		tmp3 = mono_alloc_ireg (cfg);
		tmp4 = mono_alloc_ireg (cfg);
		tmp5 = mono_alloc_ireg (cfg);

		/* add the operands */

		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg, ins->sreg1, ins->sreg2);

		/* Overflow happens if
		 *	neg - pos = pos    or
		 *	pos - neg = neg
		 * XOR of bit31 of the lhs & rhs = 1 if the signs are different
		 *
		 * tmp1 = (lhs ^ rhs)
		 * tmp2 = (lhs ^ result)
		 * if ((tmp1 < 0) & (tmp2 < 0)) then overflow
		 */

		/* tmp3 = 1 if the signs of the two inputs differ */
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp1, ins->sreg1, ins->sreg2);
		MONO_EMIT_NEW_BIALU (cfg, OP_IXOR, tmp2, ins->sreg1, ins->dreg);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_MIPS_SLTI, tmp3, tmp1, 0);
		MONO_EMIT_NEW_BIALU_IMM (cfg, OP_MIPS_SLTI, tmp4, tmp2, 0);
		MONO_EMIT_NEW_BIALU (cfg, OP_IAND, tmp5, tmp4, tmp3);

		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp5, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;

	case OP_ISUB_OVF_UN:
		tmp1 = mono_alloc_ireg (cfg);

		MONO_EMIT_NEW_BIALU (cfg, OP_ISUB, ins->dreg, ins->sreg1, ins->sreg2);
		MONO_EMIT_NEW_BIALU (cfg, OP_MIPS_SLTU, tmp1, ins->sreg1, ins->dreg);
		MONO_EMIT_NEW_COMPARE_EXC (cfg, NE_UN, tmp1, mips_zero, "OverflowException");
		ins->opcode = OP_NOP;
		break;
	}

}

static int
map_to_reg_reg_op (int op)
{
	switch (op) {
	case OP_ADD_IMM:
		return OP_IADD;
	case OP_SUB_IMM:
		return OP_ISUB;
	case OP_AND_IMM:
		return OP_IAND;
	case OP_COMPARE_IMM:
		return OP_COMPARE;
	case OP_ICOMPARE_IMM:
		return OP_ICOMPARE;
	case OP_LCOMPARE_IMM:
		return OP_LCOMPARE;
	case OP_ADDCC_IMM:
		return OP_IADDCC;
	case OP_ADC_IMM:
		return OP_IADC;
	case OP_SUBCC_IMM:
		return OP_ISUBCC;
	case OP_SBB_IMM:
		return OP_ISBB;
	case OP_OR_IMM:
		return OP_IOR;
	case OP_XOR_IMM:
		return OP_IXOR;
	case OP_MUL_IMM:
		return OP_IMUL;
	case OP_LOAD_MEMBASE:
		return OP_LOAD_MEMINDEX;
	case OP_LOADI4_MEMBASE:
		return OP_LOADI4_MEMINDEX;
	case OP_LOADU4_MEMBASE:
		return OP_LOADU4_MEMINDEX;
	case OP_LOADU1_MEMBASE:
		return OP_LOADU1_MEMINDEX;
	case OP_LOADI2_MEMBASE:
		return OP_LOADI2_MEMINDEX;
	case OP_LOADU2_MEMBASE:
		return OP_LOADU2_MEMINDEX;
	case OP_LOADI1_MEMBASE:
		return OP_LOADI1_MEMINDEX;
	case OP_LOADR4_MEMBASE:
		return OP_LOADR4_MEMINDEX;
	case OP_LOADR8_MEMBASE:
		return OP_LOADR8_MEMINDEX;
	case OP_STOREI1_MEMBASE_REG:
		return OP_STOREI1_MEMINDEX;
	case OP_STOREI2_MEMBASE_REG:
		return OP_STOREI2_MEMINDEX;
	case OP_STOREI4_MEMBASE_REG:
		return OP_STOREI4_MEMINDEX;
	case OP_STORE_MEMBASE_REG:
		return OP_STORE_MEMINDEX;
	case OP_STORER4_MEMBASE_REG:
		return OP_STORER4_MEMINDEX;
	case OP_STORER8_MEMBASE_REG:
		return OP_STORER8_MEMINDEX;
	case OP_STORE_MEMBASE_IMM:
		return OP_STORE_MEMBASE_REG;
	case OP_STOREI1_MEMBASE_IMM:
		return OP_STOREI1_MEMBASE_REG;
	case OP_STOREI2_MEMBASE_IMM:
		return OP_STOREI2_MEMBASE_REG;
	case OP_STOREI4_MEMBASE_IMM:
		return OP_STOREI4_MEMBASE_REG;
	case OP_STOREI8_MEMBASE_IMM:
		return OP_STOREI8_MEMBASE_REG;
	}
	return mono_op_imm_to_op (op);
}

static int
map_to_mips_op (int op)
{
	switch (op) {
	case OP_FBEQ:
		return OP_MIPS_FBEQ;
	case OP_FBGE:
		return OP_MIPS_FBGE;
	case OP_FBGT:
		return OP_MIPS_FBGT;
	case OP_FBLE:
		return OP_MIPS_FBLE;
	case OP_FBLT:
		return OP_MIPS_FBLT;
	case OP_FBNE_UN:
		return OP_MIPS_FBNE;
	case OP_FBGE_UN:
		return OP_MIPS_FBGE_UN;
	case OP_FBGT_UN:
		return OP_MIPS_FBGT_UN;
	case OP_FBLE_UN:
		return OP_MIPS_FBLE_UN;
	case OP_FBLT_UN:
		return OP_MIPS_FBLT_UN;

	case OP_FCEQ:
	case OP_FCGT:
	case OP_FCGT_UN:
	case OP_FCLT:
	case OP_FCLT_UN:
	default:
		g_warning ("unknown opcode %s in %s()\n", mono_inst_name (op), __FUNCTION__);
		g_assert_not_reached ();
	}
}

#define NEW_INS(cfg,after,dest,op) do {					\
		MONO_INST_NEW((cfg), (dest), (op));			\
		mono_bblock_insert_after_ins (bb, (after), (dest));	\
	} while (0)

#define INS(pos,op,_dreg,_sreg1,_sreg2) do {		\
		MonoInst *temp;						\
		MONO_INST_NEW(cfg, temp, (op));				\
		mono_bblock_insert_after_ins (bb, (pos), temp);		\
		temp->dreg = (_dreg);					\
		temp->sreg1 = (_sreg1);					\
		temp->sreg2 = (_sreg2);					\
		pos = temp;						\
	} while (0)

#define INS_IMM(pos,op,_dreg,_sreg1,_imm) do {		\
		MonoInst *temp;						\
		MONO_INST_NEW(cfg, temp, (op));				\
		mono_bblock_insert_after_ins (bb, (pos), temp);		\
		temp->dreg = (_dreg);					\
		temp->sreg1 = (_sreg1);					\
		temp->inst_c0 = (_imm);					\
		pos = temp;						\
	} while (0)

/*
 * Remove from the instruction list the instructions that can't be
 * represented with very simple instructions with no register
 * requirements.
 */
void
mono_arch_lowering_pass (MonoCompile *cfg, MonoBasicBlock *bb)
{
	MonoInst *ins, *next, *temp, *last_ins = NULL;
	int imm;

#if 1
	if (cfg->verbose_level > 2) {
		int idx = 0;

		g_print ("BASIC BLOCK %d (before lowering)\n", bb->block_num);
		MONO_BB_FOR_EACH_INS (bb, ins) {
			mono_print_ins_index (idx++, ins);
		}
		
	}
#endif

	MONO_BB_FOR_EACH_INS (bb, ins) {
loop_start:
		switch (ins->opcode) {
		case OP_COMPARE:
		case OP_ICOMPARE:
		case OP_LCOMPARE:
			next = ins->next;
			/* Branch opts can eliminate the branch */
			if (!next || (!(MONO_IS_COND_BRANCH_OP (next) || MONO_IS_COND_EXC (next) || MONO_IS_SETCC (next)))) {
				ins->opcode = OP_NOP;
				break;
			}
			break;

		case OP_COMPARE_IMM:
		case OP_ICOMPARE_IMM:
		case OP_LCOMPARE_IMM:
			next = ins->next;
			/* Branch opts can eliminate the branch */
			if (!next || (!(MONO_IS_COND_BRANCH_OP (next) || MONO_IS_COND_EXC (next) || MONO_IS_SETCC (next)))) {
				ins->opcode = OP_NOP;
				break;
			}
			if (ins->inst_imm) {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg2 = temp->dreg;
				last_ins = temp;
			}
			else {
				ins->sreg2 = mips_zero;
			}
			if (ins->opcode == OP_COMPARE_IMM)
				ins->opcode = OP_COMPARE;
			else if (ins->opcode == OP_ICOMPARE_IMM)
				ins->opcode = OP_ICOMPARE;
			else if (ins->opcode == OP_LCOMPARE_IMM)
				ins->opcode = OP_LCOMPARE;
			goto loop_start;

		case OP_IDIV_UN_IMM:
		case OP_IDIV_IMM:
		case OP_IREM_IMM:
		case OP_IREM_UN_IMM:
			NEW_INS (cfg, last_ins, temp, OP_ICONST);
			temp->inst_c0 = ins->inst_imm;
			temp->dreg = mono_alloc_ireg (cfg);
			ins->sreg2 = temp->dreg;
			if (ins->opcode == OP_IDIV_IMM)
				ins->opcode = OP_IDIV;
			else if (ins->opcode == OP_IREM_IMM)
				ins->opcode = OP_IREM;
			else if (ins->opcode == OP_IDIV_UN_IMM)
				ins->opcode = OP_IDIV_UN;
			else if (ins->opcode == OP_IREM_UN_IMM)
				ins->opcode = OP_IREM_UN;
			last_ins = temp;
			/* handle rem separately */
			goto loop_start;

#if 0
		case OP_AND_IMM:
		case OP_OR_IMM:
		case OP_XOR_IMM:
			if ((ins->inst_imm & 0xffff0000) && (ins->inst_imm & 0xffff)) {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg2 = temp->dreg;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
			}
			break;
#endif
		case OP_AND_IMM:
		case OP_IAND_IMM:
		case OP_OR_IMM:
		case OP_IOR_IMM:
		case OP_XOR_IMM:
		case OP_IXOR_IMM:
			/* unsigned 16 bit immediate */
			if (ins->inst_imm & 0xffff0000) {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg2 = temp->dreg;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
			}
			break;

		case OP_IADD_IMM:
		case OP_ADD_IMM:
		case OP_ADDCC_IMM:
			/* signed 16 bit immediate */
			if (!mips_is_imm16 (ins->inst_imm)) {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg2 = temp->dreg;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
			}
			break;

		case OP_SUB_IMM:
		case OP_ISUB_IMM:
			if (!mips_is_imm16 (-ins->inst_imm)) {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg2 = temp->dreg;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
			}
			break;

		case OP_MUL_IMM:
		case OP_IMUL_IMM:
			if (ins->inst_imm == 1) {
				ins->opcode = OP_MOVE;
				break;
			}
			if (ins->inst_imm == 0) {
				ins->opcode = OP_ICONST;
				ins->inst_c0 = 0;
				break;
			}
			imm = mono_is_power_of_two (ins->inst_imm);
			if (imm > 0) {
				ins->opcode = OP_SHL_IMM;
				ins->inst_imm = imm;
				break;
			}
			NEW_INS (cfg, last_ins, temp, OP_ICONST);
			temp->inst_c0 = ins->inst_imm;
			temp->dreg = mono_alloc_ireg (cfg);
			ins->sreg2 = temp->dreg;
			ins->opcode = map_to_reg_reg_op (ins->opcode);
			break;

		case OP_LOCALLOC_IMM:
			NEW_INS (cfg, last_ins, temp, OP_ICONST);
			temp->inst_c0 = ins->inst_imm;
			temp->dreg = mono_alloc_ireg (cfg);
			ins->sreg1 = temp->dreg;
			ins->opcode = OP_LOCALLOC;
			break;

		case OP_LOAD_MEMBASE:
		case OP_LOADI4_MEMBASE:
		case OP_LOADU4_MEMBASE:
		case OP_LOADI2_MEMBASE:
		case OP_LOADU2_MEMBASE:
		case OP_LOADI1_MEMBASE:
		case OP_LOADU1_MEMBASE:
		case OP_LOADR4_MEMBASE:
		case OP_LOADR8_MEMBASE:
		case OP_STORE_MEMBASE_REG:
		case OP_STOREI4_MEMBASE_REG:
		case OP_STOREI2_MEMBASE_REG:
		case OP_STOREI1_MEMBASE_REG:
		case OP_STORER4_MEMBASE_REG:
		case OP_STORER8_MEMBASE_REG:
			/* we can do two things: load the immed in a register
			 * and use an indexed load, or see if the immed can be
			 * represented as an ad_imm + a load with a smaller offset
			 * that fits. We just do the first for now, optimize later.
			 */
			if (mips_is_imm16 (ins->inst_offset))
				break;
			NEW_INS (cfg, last_ins, temp, OP_ICONST);
			temp->inst_c0 = ins->inst_offset;
			temp->dreg = mono_alloc_ireg (cfg);
			ins->sreg2 = temp->dreg;
			ins->opcode = map_to_reg_reg_op (ins->opcode);
			break;

		case OP_STORE_MEMBASE_IMM:
		case OP_STOREI1_MEMBASE_IMM:
		case OP_STOREI2_MEMBASE_IMM:
		case OP_STOREI4_MEMBASE_IMM:
		case OP_STOREI8_MEMBASE_IMM:
			if (!ins->inst_imm) {
				ins->sreg1 = mips_zero;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
			}
			else {
				NEW_INS (cfg, last_ins, temp, OP_ICONST);
				temp->inst_c0 = ins->inst_imm;
				temp->dreg = mono_alloc_ireg (cfg);
				ins->sreg1 = temp->dreg;
				ins->opcode = map_to_reg_reg_op (ins->opcode);
				last_ins = temp;
				goto loop_start; /* make it handle the possibly big ins->inst_offset */
			}
			break;

		case OP_FCOMPARE:
			next = ins->next;
			/* Branch opts can eliminate the branch */
			if (!next || (!(MONO_IS_COND_BRANCH_OP (next) || MONO_IS_COND_EXC (next) || MONO_IS_SETCC (next)))) {
				ins->opcode = OP_NOP;
				break;
			}
			g_assert(next);

			/*
			 * remap compare/branch and compare/set
			 * to MIPS specific opcodes.
			 */
			ins->opcode = OP_NOP;
			next->opcode = map_to_mips_op (next->opcode);
			next->sreg1 = ins->sreg1;
			next->sreg2 = ins->sreg2;
			break;

#if 0
		case OP_R8CONST:
		case OP_R4CONST:
			NEW_INS (cfg, last_ins, temp, OP_ICONST);
			temp->inst_c0 = (guint32)ins->inst_p0;
			temp->dreg = mono_alloc_ireg (cfg);
			ins->inst_basereg = temp->dreg;
			ins->inst_offset = 0;
			ins->opcode = ins->opcode == OP_R4CONST? OP_LOADR4_MEMBASE: OP_LOADR8_MEMBASE;
			last_ins = temp;
			/* make it handle the possibly big ins->inst_offset
			 * later optimize to use lis + load_membase
			 */
			goto loop_start;
#endif
		case OP_IBEQ:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_BEQ, last_ins->sreg1, last_ins->sreg2);
			last_ins->opcode = OP_NOP;
			break;

		case OP_IBNE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_BNE, last_ins->sreg1, last_ins->sreg2);
			last_ins->opcode = OP_NOP;
			break;

		case OP_IBGE:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLT, last_ins->sreg1, last_ins->sreg2);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BEQ, last_ins->dreg, mips_zero);
			break;

		case OP_IBGE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLTU, last_ins->sreg1, last_ins->sreg2);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BEQ, last_ins->dreg, mips_zero);
			break;

		case OP_IBLT:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLT, last_ins->sreg1, last_ins->sreg2);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BNE, last_ins->dreg, mips_zero);
			break;

		case OP_IBLT_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLTU, last_ins->sreg1, last_ins->sreg2);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BNE, last_ins->dreg, mips_zero);
			break;

		case OP_IBLE:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLT, last_ins->sreg2, last_ins->sreg1);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BEQ, last_ins->dreg, mips_zero);
			break;

		case OP_IBLE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLTU, last_ins->sreg2, last_ins->sreg1);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BEQ, last_ins->dreg, mips_zero);
			break;

		case OP_IBGT:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLT, last_ins->sreg2, last_ins->sreg1);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BNE, last_ins->dreg, mips_zero);
			break;

		case OP_IBGT_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(last_ins, OP_MIPS_SLTU, last_ins->sreg2, last_ins->sreg1);
			last_ins->dreg = mono_alloc_ireg (cfg);
			INS_REWRITE(ins, OP_MIPS_BNE, last_ins->dreg, mips_zero);
			break;

		case OP_CEQ:
		case OP_ICEQ:
			g_assert (ins_is_compare(last_ins));
			last_ins->opcode = OP_IXOR;
			last_ins->dreg = mono_alloc_ireg(cfg);
			INS_REWRITE_IMM(ins, OP_MIPS_SLTIU, last_ins->dreg, 1);
			break;

		case OP_CLT:
		case OP_ICLT:
			INS_REWRITE(ins, OP_MIPS_SLT, last_ins->sreg1, last_ins->sreg2);
			last_ins->opcode = OP_NOP;
			break;


		case OP_CLT_UN:
		case OP_ICLT_UN:
			INS_REWRITE(ins, OP_MIPS_SLTU, last_ins->sreg1, last_ins->sreg2);
			last_ins->opcode = OP_NOP;
			break;

		case OP_CGT:
		case OP_ICGT:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_SLT, last_ins->sreg2, last_ins->sreg1);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_CGT_UN:
		case OP_ICGT_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_SLTU, last_ins->sreg2, last_ins->sreg1);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_EQ:
		case OP_COND_EXC_IEQ:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_EQ, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_GE:
		case OP_COND_EXC_IGE:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_GE, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_GT:
		case OP_COND_EXC_IGT:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_GT, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_LE:
		case OP_COND_EXC_ILE:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_LE, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_LT:
		case OP_COND_EXC_ILT:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_LT, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_NE_UN:
		case OP_COND_EXC_INE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_NE_UN, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_GE_UN:
		case OP_COND_EXC_IGE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_GE_UN, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_GT_UN:
		case OP_COND_EXC_IGT_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_GT_UN, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_LE_UN:
		case OP_COND_EXC_ILE_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_LE_UN, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_LT_UN:
		case OP_COND_EXC_ILT_UN:
			g_assert (ins_is_compare(last_ins));
			INS_REWRITE(ins, OP_MIPS_COND_EXC_LT_UN, last_ins->sreg1, last_ins->sreg2);
			MONO_DELETE_INS(bb, last_ins);
			break;

		case OP_COND_EXC_OV:
		case OP_COND_EXC_IOV: {
			int tmp1, tmp2, tmp3, tmp4, tmp5;
			MonoInst *pos = last_ins;

			/* Overflow happens if
			 *	neg + neg = pos    or
			 *	pos + pos = neg
			 *
			 * (bit31s of operands match) AND (bit31 of operand
			 * != bit31 of result)
			 * XOR of the high bit returns 0 if the signs match
			 * XOR of that with the high bit of the result return 1
			 * if overflow.
			 */
			g_assert (last_ins->opcode == OP_IADC);

			tmp1 = mono_alloc_ireg (cfg);
			tmp2 = mono_alloc_ireg (cfg);
			tmp3 = mono_alloc_ireg (cfg);
			tmp4 = mono_alloc_ireg (cfg);
			tmp5 = mono_alloc_ireg (cfg);

			/* tmp1 = 0 if the signs of the two inputs match, else 1 */
			INS (pos, OP_IXOR, tmp1, last_ins->sreg1, last_ins->sreg2);

			/* set tmp2 = 0 if bit31 of results matches is different than the operands */
			INS (pos, OP_IXOR, tmp2, last_ins->dreg, last_ins->sreg2);
			INS (pos, OP_INOT, tmp3, tmp2, -1);

			/* OR(tmp1, tmp2) = 0 if both conditions are true */
			INS (pos, OP_IOR, tmp4, tmp3, tmp1);
			INS_IMM (pos, OP_SHR_IMM, tmp5, tmp4, 31);

			/* Now, if (tmp5 == 0) then overflow */
			INS_REWRITE(ins, OP_MIPS_COND_EXC_EQ, tmp5, mips_zero);
			ins->dreg = -1;
			break;
			}

		case OP_COND_EXC_NO:
		case OP_COND_EXC_INO:
			g_assert_not_reached ();
			break;

		case OP_COND_EXC_C:
		case OP_COND_EXC_IC:
			g_assert_not_reached ();
			break;

		case OP_COND_EXC_NC:
		case OP_COND_EXC_INC:
			g_assert_not_reached ();
			break;

		}
		last_ins = ins;
	}
	bb->last_ins = last_ins;
	bb->max_vreg = cfg->next_vreg;

#if 1
	if (cfg->verbose_level > 2) {
		int idx = 0;

		g_print ("BASIC BLOCK %d (after lowering)\n", bb->block_num);
		MONO_BB_FOR_EACH_INS (bb, ins) {
			mono_print_ins_index (idx++, ins);
		}
		
	}
#endif

}

static guchar*
emit_float_to_int (MonoCompile *cfg, guchar *code, int dreg, int sreg, int size, gboolean is_signed)
{
	/* sreg is a float, dreg is an integer reg. mips_at is used as scratch */
#if 1
	mips_truncwd (code, mips_ftemp, sreg);
#else
	mips_cvtwd (code, mips_ftemp, sreg);
#endif
	mips_mfc1 (code, dreg, mips_ftemp);
	if (!is_signed) {
		if (size == 1)
			mips_andi (code, dreg, dreg, 0xff);
		else if (size == 2) {
			mips_sll (code, dreg, dreg, 16);
			mips_srl (code, dreg, dreg, 16);
		}
	} else {
		if (size == 1) {
			mips_sll (code, dreg, dreg, 24);
			mips_sra (code, dreg, dreg, 24);
		}
		else if (size == 2) {
			mips_sll (code, dreg, dreg, 16);
			mips_sra (code, dreg, dreg, 16);
		}
	}
	return code;
}

/*
 * emit_load_volatile_arguments:
 *
 * Load volatile arguments from the stack to the original input registers.
 * Required before a tail call.
 */
static guint8 *
emit_load_volatile_arguments(MonoCompile *cfg, guint8 *code)
{
	MonoMethod *method = cfg->method;
	MonoMethodSignature *sig;
	MonoInst *inst;
	CallInfo *cinfo;
	int i;

	sig = mono_method_signature (method);
	cinfo = calculate_sizes (sig, sig->pinvoke);
	if (cinfo->struct_ret) {
		ArgInfo *ainfo = &cinfo->ret;
		inst = cfg->vret_addr;
		mips_lw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
	}

	for (i = 0; i < sig->param_count + sig->hasthis; ++i) {
		ArgInfo *ainfo = cinfo->args + i;
		inst = cfg->args [i];
		if (inst->opcode == OP_REGVAR) {
			if (ainfo->regtype == RegTypeGeneral)
				MIPS_MOVE (code, ainfo->reg, inst->dreg);
			else if (ainfo->regtype == RegTypeFP)
				g_assert_not_reached();
			else if (ainfo->regtype == RegTypeBase) {
				/* do nothing */
			} else
				g_assert_not_reached ();
		} else {
			if (ainfo->regtype == RegTypeGeneral) {
				g_assert (mips_is_imm16 (inst->inst_offset));
				switch (ainfo->size) {
				case 1:
					mips_lb (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 2:
					mips_lh (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 0: /* XXX */
				case 4:
					mips_lw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 8:
					mips_lw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					mips_lw (code, ainfo->reg + 1, inst->inst_basereg, inst->inst_offset + 4);
					break;
				default:
					g_assert_not_reached ();
					break;
				}
			} else if (ainfo->regtype == RegTypeBase) {
				/* do nothing */
			} else if (ainfo->regtype == RegTypeFP) {
				g_assert (mips_is_imm16 (inst->inst_offset));
				if (ainfo->size == 8) {
#if _MIPS_SIM == _ABIO32
					mips_lwc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset+4);
					mips_lwc1 (code, ainfo->reg+1, inst->inst_basereg, inst->inst_offset);
#elif _MIPS_SIM == _ABIN32
					mips_ldc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
#endif
				}
				else if (ainfo->size == 4)
					mips_lwc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
				else
					g_assert_not_reached ();
			} else if (ainfo->regtype == RegTypeStructByVal) {
				int i;
				int doffset = inst->inst_offset;

				g_assert (mips_is_imm16 (inst->inst_offset));
				g_assert (mips_is_imm16 (inst->inst_offset + ainfo->size * sizeof (gpointer)));
				for (i = 0; i < ainfo->size; ++i) {
					mips_lw (code, ainfo->reg + i, inst->inst_basereg, doffset);
					doffset += SIZEOF_REGISTER;
				}
			} else if (ainfo->regtype == RegTypeStructByAddr) {
				g_assert (mips_is_imm16 (inst->inst_offset));
				mips_lw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
			} else
				g_assert_not_reached ();
		}
	}

	g_free (cinfo);

	return code;
}

static guint8*
emit_reserve_param_area (MonoCompile *cfg, guint8 *code)
{
	int size = cfg->param_area;

	size += MONO_ARCH_FRAME_ALIGNMENT - 1;
	size &= -MONO_ARCH_FRAME_ALIGNMENT;

	if (!size)
		return code;
#if 0
	ppc_lwz (code, ppc_r0, 0, ppc_sp);
	if (ppc_is_imm16 (-size)) {
		ppc_stwu (code, ppc_r0, -size, ppc_sp);
	} else {
		ppc_load (code, ppc_r11, -size);
		ppc_stwux (code, ppc_r0, ppc_sp, ppc_r11);
	}
#endif
	return code;
}

static guint8*
emit_unreserve_param_area (MonoCompile *cfg, guint8 *code)
{
	int size = cfg->param_area;

	size += MONO_ARCH_FRAME_ALIGNMENT - 1;
	size &= -MONO_ARCH_FRAME_ALIGNMENT;

	if (!size)
		return code;
#if 0
	ppc_lwz (code, ppc_r0, 0, ppc_sp);
	if (ppc_is_imm16 (size)) {
		ppc_stwu (code, ppc_r0, size, ppc_sp);
	} else {
		ppc_load (code, ppc_r11, size);
		ppc_stwux (code, ppc_r0, ppc_sp, ppc_r11);
	}
#endif
	return code;
}

void
mono_arch_output_basic_block (MonoCompile *cfg, MonoBasicBlock *bb)
{
	MonoInst *ins;
	MonoCallInst *call;
	guint offset;
	guint8 *code = cfg->native_code + cfg->code_len;
	MonoInst *last_ins = NULL;
	guint last_offset = 0;
	int max_len, cpos;
	int ins_cnt = 0;

	/* we don't align basic blocks of loops on mips */

	if (cfg->verbose_level > 2)
		g_print ("Basic block %d starting at offset 0x%x\n", bb->block_num, bb->native_offset);

	cpos = bb->max_offset;

#if 0
	if (cfg->prof_options & MONO_PROFILE_COVERAGE) {
		MonoCoverageInfo *cov = mono_get_coverage_info (cfg->method);
		g_assert (!mono_compile_aot);
		cpos += 20;
		if (bb->cil_code)
			cov->data [bb->dfn].iloffset = bb->cil_code - cfg->cil_code;
		/* this is not thread save, but good enough */
		/* fixme: howto handle overflows? */
		mips_load_const (code, mips_at, &cov->data [bb->dfn].count);
		mips_lw (code, mips_temp, mips_at, 0);
		mips_addiu (code, mips_temp, mips_temp, 1);
		mips_sw (code, mips_temp, mips_at, 0);
	}
#endif
	MONO_BB_FOR_EACH_INS (bb, ins) {
		offset = code - cfg->native_code;

		max_len = ((guint8 *)ins_get_spec (ins->opcode))[MONO_INST_LEN];

		if (offset > (cfg->code_size - max_len - 16)) {
			cfg->code_size *= 2;
			cfg->native_code = g_realloc (cfg->native_code, cfg->code_size);
			code = cfg->native_code + offset;
		}
		mono_debug_record_line_number (cfg, ins, offset);
		if (cfg->verbose_level > 2) {
			g_print ("    @ 0x%x\t", offset);
			mono_print_ins_index (ins_cnt++, ins);
		}
		/* Check for virtual regs that snuck by */
		g_assert ((ins->dreg >= -1) && (ins->dreg < 32));

		switch (ins->opcode) {
		case OP_RELAXED_NOP:
		case OP_NOP:
		case OP_DUMMY_USE:
		case OP_DUMMY_STORE:
		case OP_NOT_REACHED:
		case OP_NOT_NULL:
			break;
		case OP_TLS_GET:
			g_assert_not_reached();
#if 0
			emit_tls_access (code, ins->dreg, ins->inst_offset);
#endif
			break;
		case OP_BIGMUL:
			mips_mult (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			mips_mfhi (code, ins->dreg+1);
			break;
		case OP_BIGMUL_UN:
			mips_multu (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			mips_mfhi (code, ins->dreg+1);
			break;
		case OP_MEMORY_BARRIER:
#if 0
			ppc_sync (code);
#endif
			break;
		case OP_STOREI1_MEMBASE_IMM:
			mips_load_const (code, mips_temp, ins->inst_imm);
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sb (code, mips_temp, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_sb (code, mips_temp, mips_at, ins->inst_destbasereg);
			}
			break;
		case OP_STOREI2_MEMBASE_IMM:
			mips_load_const (code, mips_temp, ins->inst_imm);
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sh (code, mips_temp, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_sh (code, mips_temp, mips_at, ins->inst_destbasereg);
			}
			break;
		case OP_STOREI8_MEMBASE_IMM:
			mips_load_const (code, mips_temp, ins->inst_imm);
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sd (code, mips_temp, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_sd (code, mips_temp, mips_at, ins->inst_destbasereg);
			}
			break;
		case OP_STORE_MEMBASE_IMM:
		case OP_STOREI4_MEMBASE_IMM:
			mips_load_const (code, mips_temp, ins->inst_imm);
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sw (code, mips_temp, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_sw (code, mips_temp, mips_at, ins->inst_destbasereg);
			}
			break;
		case OP_STOREI1_MEMBASE_REG:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sb (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_destbasereg);
				mips_sb (code, ins->sreg1, mips_at, 0);
			}
			break;
		case OP_STOREI2_MEMBASE_REG:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sh (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_destbasereg);
				mips_sh (code, ins->sreg1, mips_at, 0);
			}
			break;
		case OP_STORE_MEMBASE_REG:
		case OP_STOREI4_MEMBASE_REG:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sw (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_destbasereg);
				mips_sw (code, ins->sreg1, mips_at, 0);
			}
			break;
		case OP_STOREI8_MEMBASE_REG:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_sd (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_destbasereg);
				mips_sd (code, ins->sreg1, mips_at, 0);
			}
			break;
		case OP_LOADU4_MEM:
			g_assert_not_reached ();
			//x86_mov_reg_imm (code, ins->dreg, ins->inst_p0);
			//x86_mov_reg_membase (code, ins->dreg, ins->dreg, 0, 4);
			break;
		case OP_LOADI8_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_ld (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_ld (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_LOAD_MEMBASE:
		case OP_LOADI4_MEMBASE:
		case OP_LOADU4_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_lw (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lw (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_LOADI1_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_lb (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lb (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_LOADU1_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_lbu (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lbu (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_LOADI2_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_lh (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lh (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_LOADU2_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
				mips_lhu (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lhu (code, ins->dreg, mips_at, 0);
			}
			break;
		case OP_ICONV_TO_I1:
			mips_sll (code, mips_at, ins->sreg1, 24);
			mips_sra (code, ins->dreg, mips_at, 24);
			break;
		case OP_ICONV_TO_I2:
			mips_sll (code, mips_at, ins->sreg1, 16);
			mips_sra (code, ins->dreg, mips_at, 16);
			break;
		case OP_ICONV_TO_U1:
			mips_andi (code, ins->dreg, ins->sreg1, 0xff);
			break;
		case OP_ICONV_TO_U2:
			mips_sll (code, mips_at, ins->sreg1, 16);
			mips_srl (code, ins->dreg, mips_at, 16);
			break;
		case OP_MIPS_SLT:
			mips_slt (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_MIPS_SLTI:
			g_assert (mips_is_imm16 (ins->inst_imm));
			mips_slti (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;
		case OP_MIPS_SLTU:
			mips_sltu (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_MIPS_SLTIU:
			g_assert (mips_is_imm16 (ins->inst_imm));
			mips_sltiu (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;
		case OP_BREAK:
			mips_break (code, 0xfd);
			break;
		case OP_IADD:
			mips_addu (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_LADD:
			mips_daddu (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;

		case OP_ADD_IMM:
		case OP_IADD_IMM:
			g_assert (mips_is_imm16 (ins->inst_imm));
			mips_addiu (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;
		case OP_LADD_IMM:
			g_assert (mips_is_imm16 (ins->inst_imm));
			mips_daddiu (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;

		case OP_ISUB:
			mips_subu (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_LSUB:
			mips_dsubu (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;

		case OP_ISUB_IMM:
		case OP_SUB_IMM:
			// we add the negated value
			g_assert (mips_is_imm16 (-ins->inst_imm));
			mips_addiu (code, ins->dreg, ins->sreg1, -ins->inst_imm);
			break;

		case OP_LSUB_IMM:
			// we add the negated value
			g_assert (mips_is_imm16 (-ins->inst_imm));
			mips_daddiu (code, ins->dreg, ins->sreg1, -ins->inst_imm);
			break;

		case OP_IAND:
		case OP_LAND:
			mips_and (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;

		case OP_AND_IMM:
		case OP_IAND_IMM:
		case OP_LAND_IMM:
			g_assert (!(ins->inst_imm & 0xffff0000));
			mips_andi (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;

		case OP_IDIV:
		case OP_IREM: {
			guint32 *divisor_is_m1;
			guint32 *divisor_is_zero;

			/* */
			mips_addiu (code, mips_at, mips_zero, 0xffff);
			divisor_is_m1 = (guint32 *)(void *)code;
			mips_bne (code, ins->sreg2, mips_at, 0);
			mips_nop (code);

			/* Divide by -1 -- throw exception */
			EMIT_SYSTEM_EXCEPTION_NAME("ArithmeticException");

			mips_patch (divisor_is_m1, (guint32)code);

			/* Put divide in branch delay slot (NOT YET) */
			divisor_is_zero = (guint32 *)(void *)code;
			mips_bne (code, ins->sreg2, mips_zero, 0);
			mips_nop (code);

			/* Divide by zero -- throw exception */
			EMIT_SYSTEM_EXCEPTION_NAME("DivideByZeroException");

			mips_patch (divisor_is_zero, (guint32)code);
			mips_div (code, ins->sreg1, ins->sreg2);
			if (ins->opcode == OP_IDIV)
				mips_mflo (code, ins->dreg);
			else
				mips_mfhi (code, ins->dreg);
			break;
		}
		case OP_IDIV_UN: 
		case OP_IREM_UN: {
			guint32 *divisor_is_zero = (guint32 *)(void *)code;

			/* Put divide in branch delay slot (NOT YET) */
			mips_bne (code, ins->sreg2, mips_zero, 0);
			mips_nop (code);

			/* Divide by zero -- throw exception */
			EMIT_SYSTEM_EXCEPTION_NAME("DivideByZeroException");

			mips_patch (divisor_is_zero, (guint32)code);
			mips_divu (code, ins->sreg1, ins->sreg2);
			if (ins->opcode == OP_IDIV_UN)
				mips_mflo (code, ins->dreg);
			else
				mips_mfhi (code, ins->dreg);
			break;
		}
		case OP_DIV_IMM:
			g_assert_not_reached ();
#if 0
			ppc_load (code, ppc_r11, ins->inst_imm);
			ppc_divwod (code, ins->dreg, ins->sreg1, ppc_r11);
			ppc_mfspr (code, ppc_r0, ppc_xer);
			ppc_andisd (code, ppc_r0, ppc_r0, (1<<14));
			/* FIXME: use OverflowException for 0x80000000/-1 */
			EMIT_COND_SYSTEM_EXCEPTION_FLAGS (PPC_BR_FALSE, PPC_BR_EQ, "DivideByZeroException");
#endif
			g_assert_not_reached();
			break;
		case OP_REM_IMM:
			g_assert_not_reached ();
		case OP_IOR:
			mips_or (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_OR_IMM:
		case OP_IOR_IMM:
			g_assert (!(ins->inst_imm & 0xffff0000));
			mips_ori (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;
		case OP_IXOR:
			mips_xor (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_XOR_IMM:
		case OP_IXOR_IMM:
			/* unsigned 16-bit immediate */
			g_assert (!(ins->inst_imm & 0xffff0000));
			mips_xori (code, ins->dreg, ins->sreg1, ins->inst_imm);
			break;
		case OP_ISHL:
			mips_sllv (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_SHL_IMM:
		case OP_ISHL_IMM:
			mips_sll (code, ins->dreg, ins->sreg1, ins->inst_imm & 0x1f);
			break;
		case OP_ISHR:
			mips_srav (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_LSHR:
			mips_dsrav (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_SHR_IMM:
		case OP_ISHR_IMM:
			mips_sra (code, ins->dreg, ins->sreg1, ins->inst_imm & 0x1f);
			break;
		case OP_LSHR_IMM:
			mips_dsra (code, ins->dreg, ins->sreg1, ins->inst_imm & 0x3f);
			break;
		case OP_SHR_UN_IMM:
		case OP_ISHR_UN_IMM:
			mips_srl (code, ins->dreg, ins->sreg1, ins->inst_imm & 0x1f);
			break;
		case OP_LSHR_UN_IMM:
			mips_dsrl (code, ins->dreg, ins->sreg1, ins->inst_imm & 0x3f);
			break;
		case OP_ISHR_UN:
			mips_srlv (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_LSHR_UN:
			mips_dsrlv (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_INOT:
		case OP_LNOT:
			mips_nor (code, ins->dreg, mips_zero, ins->sreg1);
			break;
		case OP_INEG:
			mips_subu (code, ins->dreg, mips_zero, ins->sreg1);
			break;
		case OP_LNEG:
			mips_dsubu (code, ins->dreg, mips_zero, ins->sreg1);
			break;
		case OP_IMUL:
#if USE_MUL
			mips_mul (code, ins->dreg, ins->sreg1, ins->sreg2);
#else
			mips_mult (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			mips_nop (code);
			mips_nop (code);
#endif
			break;
#if SIZEOF_REGISTER == 8
		case OP_LMUL:
			mips_dmult (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			break;
#endif
		case OP_IMUL_OVF: {
			guint32 *patch;
			mips_mult (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			mips_mfhi (code, mips_at);
			mips_nop (code);
			mips_nop (code);
			mips_sra (code, mips_temp, ins->dreg, 31);
			patch = (guint32 *)(void *)code;
			mips_beq (code, mips_temp, mips_at, 0);
			mips_nop (code);
			EMIT_SYSTEM_EXCEPTION_NAME("OverflowException");
			mips_patch (patch, (guint32)code);
			break;
		}
		case OP_IMUL_OVF_UN:
#if 0
			mips_mul (code, ins->dreg, ins->sreg1, ins->sreg2);
#else
			mips_mult (code, ins->sreg1, ins->sreg2);
			mips_mflo (code, ins->dreg);
			mips_mfhi (code, mips_at);
			mips_nop (code);
			mips_nop (code);
#endif
			/* XXX - Throw exception if we overflowed */
			break;
		case OP_ICONST:
			mips_load_const (code, ins->dreg, ins->inst_c0);
			break;
#if SIZEOF_REGISTER == 8
		case OP_I8CONST:
			mips_load_const (code, ins->dreg, ins->inst_c0);
			break;
#endif
		case OP_AOTCONST:
			mono_add_patch_info (cfg, offset, (MonoJumpInfoType)ins->inst_i1, ins->inst_p0);
			mips_load (code, ins->dreg, 0);
			break;

		case OP_MIPS_MTC1S:
			mips_mtc1 (code, ins->dreg, ins->sreg1);
			break;
		case OP_MIPS_MTC1S_2:
			mips_mtc1 (code, ins->dreg, ins->sreg1);
			mips_mtc1 (code, ins->dreg+1, ins->sreg2);
			break;
		case OP_MIPS_MFC1S:
			mips_mfc1 (code, ins->dreg, ins->sreg1);
			break;
		case OP_MIPS_MTC1D:
			mips_dmtc1 (code, ins->dreg, ins->sreg1);
			break;
		case OP_MIPS_MFC1D:
#if 0
			mips_dmfc1 (code, ins->dreg, ins->sreg1);
#else
			mips_mfc1 (code, ins->dreg+1, ins->sreg1);
			mips_mfc1 (code, ins->dreg, ins->sreg1+1);
#endif
			break;

		case OP_ICONV_TO_I4:
		case OP_ICONV_TO_U4:
		case OP_MOVE:
			if (ins->dreg != ins->sreg1)
				MIPS_MOVE (code, ins->dreg, ins->sreg1);
			break;
#if SIZEOF_REGISTER == 8
		case OP_ZEXT_I4:
			mips_dsll (code, ins->dreg, ins->sreg1, 32);
			mips_dsrl (code, ins->dreg, ins->dreg, 32);
			break;
		case OP_SEXT_I4:
			mips_dsll (code, ins->dreg, ins->sreg1, 32);
			mips_dsra (code, ins->dreg, ins->dreg, 32);
			break;
#endif
		case OP_SETLRET:
			/* Get sreg1 into v1, sreg2 into v0 */

			if (ins->sreg1 == mips_v0) {
				if (ins->sreg1 != mips_at)
					MIPS_MOVE (code, mips_at, ins->sreg1);
				if (ins->sreg2 != mips_v0)
					MIPS_MOVE (code, mips_v0, ins->sreg2);
				MIPS_MOVE (code, mips_v1, mips_at);
			}
			else {
				if (ins->sreg2 != mips_v0)
					MIPS_MOVE (code, mips_v0, ins->sreg2);
				if (ins->sreg1 != mips_v1)
					MIPS_MOVE (code, mips_v1, ins->sreg1);
			}
			break;
		case OP_FMOVE:
			if (ins->dreg != ins->sreg1) {
				mips_fmovd (code, ins->dreg, ins->sreg1);
			}
			break;
		case OP_MIPS_CVTSD:
			/* Convert from double to float and leave it there */
			mips_cvtsd (code, ins->dreg, ins->sreg1);
			break;
		case OP_FCONV_TO_R4:
#if 0
			mips_cvtsd (code, ins->dreg, ins->sreg1);
#else
			/* Just a move, no precision change */
			if (ins->dreg != ins->sreg1) {
				mips_fmovd (code, ins->dreg, ins->sreg1);
			}
#endif
			break;
		case OP_JMP:
			code = emit_load_volatile_arguments(cfg, code);

			/*
			 * Pop our stack, then jump to specified method (tail-call)
			 * Keep in sync with mono_arch_emit_epilog
			 */
			code = mono_arch_emit_epilog_sub (cfg, code);

			mono_add_patch_info (cfg, (guint8*) code - cfg->native_code,
					     MONO_PATCH_INFO_METHOD_JUMP, ins->inst_p0);
#if LONG_BRANCH
			mips_lui (code, mips_t9, mips_zero, 0);
			mips_addiu (code, mips_t9, mips_t9, 0);
			mips_jr (code, mips_t9);
			mips_nop (code);
#else
			mips_beq (code, mips_zero, mips_zero, 0);
			mips_nop (code);
#endif
			break;
		case OP_CHECK_THIS:
			/* ensure ins->sreg1 is not NULL */
			mips_lw (code, mips_zero, ins->sreg1, 0);
			break;
		case OP_ARGLIST: {
			if (mips_is_imm16 (cfg->sig_cookie + cfg->stack_usage)) {
				mips_addiu (code, mips_at, cfg->frame_reg, cfg->sig_cookie + cfg->stack_usage);
			} else {
				mips_load_const (code, mips_at, cfg->sig_cookie + cfg->stack_usage);
				mips_addu (code, mips_at, cfg->frame_reg, mips_at);
			}
			mips_sw (code, mips_at, ins->sreg1, 0);
			break;
		}
		case OP_FCALL:
		case OP_LCALL:
		case OP_VCALL:
		case OP_VCALL2:
		case OP_VOIDCALL:
		case OP_CALL:
		case OP_FCALL_REG:
		case OP_LCALL_REG:
		case OP_VCALL_REG:
		case OP_VCALL2_REG:
		case OP_VOIDCALL_REG:
		case OP_CALL_REG:
		case OP_FCALL_MEMBASE:
		case OP_LCALL_MEMBASE:
		case OP_VCALL_MEMBASE:
		case OP_VCALL2_MEMBASE:
		case OP_VOIDCALL_MEMBASE:
		case OP_CALL_MEMBASE:
			call = (MonoCallInst*)ins;
			switch (ins->opcode) {
			case OP_FCALL:
			case OP_LCALL:
			case OP_VCALL:
			case OP_VCALL2:
			case OP_VOIDCALL:
			case OP_CALL:
				if (ins->flags & MONO_INST_HAS_METHOD)
					mono_add_patch_info (cfg, offset, MONO_PATCH_INFO_METHOD, call->method);
				else
					mono_add_patch_info (cfg, offset, MONO_PATCH_INFO_ABS, call->fptr);
				mips_lui (code, mips_t9, mips_zero, 0);
				mips_addiu (code, mips_t9, mips_t9, 0);
				break;
			case OP_FCALL_REG:
			case OP_LCALL_REG:
			case OP_VCALL_REG:
			case OP_VCALL2_REG:
			case OP_VOIDCALL_REG:
			case OP_CALL_REG:
				MIPS_MOVE (code, mips_t9, ins->sreg1);
				break;
			case OP_FCALL_MEMBASE:
			case OP_LCALL_MEMBASE:
			case OP_VCALL_MEMBASE:
			case OP_VCALL2_MEMBASE:
			case OP_VOIDCALL_MEMBASE:
			case OP_CALL_MEMBASE:
				mips_lw (code, mips_t9, ins->sreg1, ins->inst_offset);
				break;
			}
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
#if PROMOTE_R4_TO_R8
			/* returned an FP R4 (single), promote to R8 (double) in place */
			if ((ins->opcode == OP_FCALL ||
			     ins->opcode == OP_FCALL_REG) &&
			    call->signature->ret->type == MONO_TYPE_R4) {
				mips_cvtds (code, mips_f0, mips_f0);
			}
#endif
			break;
		case OP_LOCALLOC: {
			int area_offset = cfg->param_area;

			/* Round up ins->sreg1, mips_at ends up holding size */
			mips_addiu (code, mips_at, ins->sreg1, 31);
			mips_addiu (code, mips_temp, mips_zero, ~31);
			mips_and (code, mips_at, mips_at, mips_temp);

			mips_subu (code, mips_sp, mips_sp, mips_at);
			g_assert (mips_is_imm16 (area_offset));
			mips_addiu (code, ins->dreg, mips_sp, area_offset);

			if (ins->flags & MONO_INST_INIT) {
				mips_move (code, mips_temp, ins->dreg);
				mips_sb (code, mips_zero, mips_temp, 0);
				mips_addiu (code, mips_at, mips_at, -1);
				mips_bne (code, mips_at, mips_zero, -3);
				mips_addiu (code, mips_temp, mips_temp, 1);
			}
			break;
		}
		case OP_THROW: {
			gpointer addr = mono_arch_get_throw_exception();
			mips_move (code, mips_a0, ins->sreg1);
			mips_load_const (code, mips_t9, addr);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
			mips_break (code, 0xfc);
			break;
		}
		case OP_RETHROW: {
			gpointer addr = mono_arch_get_rethrow_exception();
			mips_move (code, mips_a0, ins->sreg1);
			mips_load_const (code, mips_t9, addr);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
			mips_break (code, 0xfb);
			break;
		}
		case OP_START_HANDLER: {
			/*
			 * The START_HANDLER instruction marks the beginning of
			 * a handler block. It is called using a call
			 * instruction, so mips_ra contains the return address.
			 * Since the handler executes in the same stack frame
			 * as the method itself, we can't use save/restore to
			 * save the return address. Instead, we save it into
			 * a dedicated variable.
			 */
			MonoInst *spvar = mono_find_spvar_for_region (cfg, bb->region);
			g_assert (spvar->inst_basereg != mips_sp);
			code = emit_reserve_param_area (cfg, code);

			if (mips_is_imm16 (spvar->inst_offset)) {
				mips_sw (code, mips_ra, spvar->inst_basereg, spvar->inst_offset);
			} else {
				mips_load_const (code, mips_at, spvar->inst_offset);
				mips_addu (code, mips_at, mips_at, spvar->inst_basereg);
				mips_sw (code, mips_ra, mips_at, 0);
			}
			break;
		}
		case OP_ENDFILTER: {
			MonoInst *spvar = mono_find_spvar_for_region (cfg, bb->region);
			g_assert (spvar->inst_basereg != mips_sp);
			code = emit_unreserve_param_area (cfg, code);

			if (ins->sreg1 != mips_v0)
				MIPS_MOVE (code, mips_v0, ins->sreg1);
			if (mips_is_imm16 (spvar->inst_offset)) {
				mips_lw (code, mips_ra, spvar->inst_basereg, spvar->inst_offset);
			} else {
				mips_load_const (code, mips_at, spvar->inst_offset);
				mips_addu (code, mips_at, mips_at, spvar->inst_basereg);
				mips_lw (code, mips_ra, mips_at, 0);
			}
			mips_jr (code, mips_ra);
			mips_nop (code);
			break;
		}
		case OP_ENDFINALLY: {
			MonoInst *spvar = mono_find_spvar_for_region (cfg, bb->region);
			g_assert (spvar->inst_basereg != mips_sp);
			code = emit_unreserve_param_area (cfg, code);
			mips_lw (code, mips_t9, spvar->inst_basereg, spvar->inst_offset);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
			break;
		}
		case OP_CALL_HANDLER: 
			mono_add_patch_info (cfg, offset, MONO_PATCH_INFO_BB, ins->inst_target_bb);
			mips_lui (code, mips_t9, mips_zero, 0);
			mips_addiu (code, mips_t9, mips_t9, 0);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
			break;
		case OP_LABEL:
			ins->inst_c0 = code - cfg->native_code;
			break;
		case OP_BR:
			mono_add_patch_info (cfg, offset, MONO_PATCH_INFO_BB, ins->inst_target_bb);
#if LONG_BRANCH
			mips_lui (code, mips_at, mips_zero, 0);
			mips_addiu (code, mips_at, mips_at, 0);
			mips_jr (code, mips_at);
			mips_nop (code);
#else
			mips_beq (code, mips_zero, mips_zero, 0);
			mips_nop (code);
#endif
			break;
		case OP_BR_REG:
			mips_jr (code, ins->sreg1);
			mips_nop (code);
			break;
		case OP_SWITCH: {
			int i;

			max_len += 4 * GPOINTER_TO_INT (ins->klass);
			if (offset > (cfg->code_size - max_len - 16)) {
				cfg->code_size += max_len;
				cfg->code_size *= 2;
				cfg->native_code = g_realloc (cfg->native_code, cfg->code_size);
				code = cfg->native_code + offset;
			}
			g_assert (ins->sreg1 != -1);
			mips_sll (code, mips_at, ins->sreg1, 2);
			if (1 || !(cfg->flags & MONO_CFG_HAS_CALLS))
				MIPS_MOVE (code, mips_t8, mips_ra);
			mips_bgezal (code, mips_zero, 1);	/* bal */
			mips_nop (code);
			mips_addu (code, mips_t9, mips_ra, mips_at);
			/* Table is 16 or 20 bytes from target of bal above */
			if (1 || !(cfg->flags & MONO_CFG_HAS_CALLS)) {
				MIPS_MOVE (code, mips_ra, mips_t8);
				mips_lw (code, mips_t9, mips_t9, 20);
			}
			else
				mips_lw (code, mips_t9, mips_t9, 16);
			mips_jalr (code, mips_t9, mips_t8);
			mips_nop (code);
			for (i = 0; i < GPOINTER_TO_INT (ins->klass); ++i)
				mips_emit32 (code, 0xfefefefe);
			break;
		}
		case OP_CEQ:
		case OP_ICEQ:
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_beq (code, mips_at, mips_zero, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;
		case OP_CLT:
		case OP_CLT_UN:
		case OP_ICLT:
		case OP_ICLT_UN:
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_bltz (code, mips_at, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;
		case OP_CGT:
		case OP_CGT_UN:
		case OP_ICGT:
		case OP_ICGT_UN:
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_bgtz (code, mips_at, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;

		case OP_MIPS_COND_EXC_EQ:
		case OP_MIPS_COND_EXC_GE:
		case OP_MIPS_COND_EXC_GT:
		case OP_MIPS_COND_EXC_LE:
		case OP_MIPS_COND_EXC_LT:
		case OP_MIPS_COND_EXC_NE_UN:
		case OP_MIPS_COND_EXC_GE_UN:
		case OP_MIPS_COND_EXC_GT_UN:
		case OP_MIPS_COND_EXC_LE_UN:
		case OP_MIPS_COND_EXC_LT_UN:

		case OP_MIPS_COND_EXC_OV:
		case OP_MIPS_COND_EXC_NO:
		case OP_MIPS_COND_EXC_C:
		case OP_MIPS_COND_EXC_NC:

		case OP_MIPS_COND_EXC_IEQ:
		case OP_MIPS_COND_EXC_IGE:
		case OP_MIPS_COND_EXC_IGT:
		case OP_MIPS_COND_EXC_ILE:
		case OP_MIPS_COND_EXC_ILT:
		case OP_MIPS_COND_EXC_INE_UN:
		case OP_MIPS_COND_EXC_IGE_UN:
		case OP_MIPS_COND_EXC_IGT_UN:
		case OP_MIPS_COND_EXC_ILE_UN:
		case OP_MIPS_COND_EXC_ILT_UN:

		case OP_MIPS_COND_EXC_IOV:
		case OP_MIPS_COND_EXC_INO:
		case OP_MIPS_COND_EXC_IC:
		case OP_MIPS_COND_EXC_INC: {
			guint32 *skip;
			guint32 *throw;

			/* If the condition is true, raise the exception */

			/* need to reverse test to skip around exception raising */

			/* For the moment, branch around a branch to avoid reversing
			   the tests. */

			/* Remember, an unpatched branch to 0 branches to the delay slot */
			switch (ins->opcode) {
			case OP_MIPS_COND_EXC_EQ:
				throw = (guint32 *)(void *)code;
				mips_beq (code, ins->sreg1, ins->sreg2, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_NE_UN:
				throw = (guint32 *)(void *)code;
				mips_bne (code, ins->sreg1, ins->sreg2, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_LE_UN:
				mips_subu (code, mips_at, ins->sreg1, ins->sreg2);
				throw = (guint32 *)(void *)code;
				mips_blez (code, mips_at, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_GT:
				mips_slt (code, mips_at, ins->sreg2, ins->sreg1);
				throw = (guint32 *)(void *)code;
				mips_bne (code, mips_at, mips_zero, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_GT_UN:
				mips_sltu (code, mips_at, ins->sreg2, ins->sreg1);
				throw = (guint32 *)(void *)code;
				mips_bne (code, mips_at, mips_zero, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_LT:
				mips_slt (code, mips_at, ins->sreg1, ins->sreg2);
				throw = (guint32 *)(void *)code;
				mips_bne (code, mips_at, mips_zero, 0);
				mips_nop (code);
				break;

			case OP_MIPS_COND_EXC_LT_UN:
				mips_sltu (code, mips_at, ins->sreg1, ins->sreg2);
				throw = (guint32 *)(void *)code;
				mips_bne (code, mips_at, mips_zero, 0);
				mips_nop (code);
				break;

			default:
				/* Not yet implemented */
				g_warning ("NYI conditional exception %s\n", mono_inst_name (ins->opcode));
				g_assert_not_reached ();
			}
			skip = (guint32 *)(void *)code;
			mips_beq (code, mips_zero, mips_zero, 0);
			mips_nop (code);
			mips_patch (throw, (guint32)code);
			code = mips_emit_exc_by_name (code, ins->inst_p1);
			mips_patch (skip, (guint32)code);
			cfg->bb_exit->max_offset += 24;
			break;
		}
		case OP_MIPS_BEQ:
		case OP_MIPS_BNE:
		case OP_MIPS_BGEZ:
		case OP_MIPS_BGTZ:
		case OP_MIPS_BLEZ:
		case OP_MIPS_BLTZ:
			code = mips_emit_cond_branch (cfg, code, ins->opcode, ins);
			break;

		/* floating point opcodes */
		case OP_R8CONST:
#if 0
			if (((guint32)ins->inst_p0) & (1 << 15))
				mips_lui (code, mips_at, mips_zero, (((guint32)ins->inst_p0)>>16)+1);
			else
				mips_lui (code, mips_at, mips_zero, (((guint32)ins->inst_p0)>>16));
			mips_ldc1 (code, ins->dreg, mips_at, ((guint32)ins->inst_p0) & 0xffff);
#else
			mips_load_const (code, mips_at, ins->inst_p0);
			mips_lwc1 (code, ins->dreg, mips_at, 4);
			mips_lwc1 (code, ins->dreg+1, mips_at, 0);
#endif
			break;
		case OP_R4CONST:
			if (((guint32)ins->inst_p0) & (1 << 15))
				mips_lui (code, mips_at, mips_zero, (((guint32)ins->inst_p0)>>16)+1);
			else
				mips_lui (code, mips_at, mips_zero, (((guint32)ins->inst_p0)>>16));
			mips_lwc1 (code, ins->dreg, mips_at, ((guint32)ins->inst_p0) & 0xffff);
#if PROMOTE_R4_TO_R8
			mips_cvtds (code, ins->dreg, ins->dreg);
#endif
			break;
		case OP_STORER8_MEMBASE_REG:
			if (mips_is_imm16 (ins->inst_offset)) {
#if _MIPS_SIM == _ABIO32
				mips_swc1 (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset+4);
				mips_swc1 (code, ins->sreg1+1, ins->inst_destbasereg, ins->inst_offset);
#elif _MIPS_SIM == _ABIN32
				mips_sdc1 (code, ins->sreg1, ins->inst_destbasereg, ins->inst_offset);
#endif
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_destbasereg);
				mips_swc1 (code, ins->sreg1, mips_at, 4);
				mips_swc1 (code, ins->sreg1+1, mips_at, 0);
			}
			break;
		case OP_LOADR8_MEMBASE:
			if (mips_is_imm16 (ins->inst_offset)) {
#if _MIPS_SIM == _ABIO32
				mips_lwc1 (code, ins->dreg, ins->inst_basereg, ins->inst_offset+4);
				mips_lwc1 (code, ins->dreg+1, ins->inst_basereg, ins->inst_offset);
#elif _MIPS_SIM == _ABIN32
				mips_ldc1 (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
#endif
			} else {
				mips_load_const (code, mips_at, ins->inst_offset);
				mips_addu (code, mips_at, mips_at, ins->inst_basereg);
				mips_lwc1 (code, ins->dreg, mips_at, 4);
				mips_lwc1 (code, ins->dreg+1, mips_at, 0);
			}
			break;
		case OP_STORER4_MEMBASE_REG:
			g_assert (mips_is_imm16 (ins->inst_offset));
#if PROMOTE_R4_TO_R8
			/* Need to convert ins->sreg1 to single-precision first */
			mips_cvtsd (code, mips_ftemp, ins->sreg1);
#endif
			mips_swc1 (code, mips_ftemp, ins->inst_destbasereg, ins->inst_offset);
			break;
		case OP_MIPS_LWC1:
			g_assert (mips_is_imm16 (ins->inst_offset));
			mips_lwc1 (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
			break;
		case OP_LOADR4_MEMBASE:
			g_assert (mips_is_imm16 (ins->inst_offset));
			mips_lwc1 (code, ins->dreg, ins->inst_basereg, ins->inst_offset);
#if PROMOTE_R4_TO_R8
			/* Convert to double precision in place */
			mips_cvtds (code, ins->dreg, ins->dreg);
#endif
			break;
		case OP_ICONV_TO_R_UN: {
			static const guint64 adjust_val = 0x41F0000000000000ULL;

			/* convert unsigned int to double */
			mips_mtc1 (code, mips_ftemp, ins->sreg1);
			mips_bgez (code, ins->sreg1, 5);
			mips_cvtdw (code, ins->dreg, mips_ftemp);

			mips_load (code, mips_at, (guint32) &adjust_val);
			mips_ldc1  (code, mips_ftemp, mips_at, 0);
			mips_faddd (code, ins->dreg, ins->dreg, mips_ftemp);
			/* target is here */
			break;
		}
		case OP_ICONV_TO_R4:
			mips_mtc1 (code, mips_ftemp, ins->sreg1);
			mips_cvtsw (code, ins->dreg, mips_ftemp);
			mips_cvtds (code, ins->dreg, ins->dreg);
			break;
		case OP_ICONV_TO_R8:
			mips_mtc1 (code, mips_ftemp, ins->sreg1);
			mips_cvtdw (code, ins->dreg, mips_ftemp);
			break;
		case OP_FCONV_TO_I1:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 1, TRUE);
			break;
		case OP_FCONV_TO_U1:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 1, FALSE);
			break;
		case OP_FCONV_TO_I2:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 2, TRUE);
			break;
		case OP_FCONV_TO_U2:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 2, FALSE);
			break;
		case OP_FCONV_TO_I4:
		case OP_FCONV_TO_I:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 4, TRUE);
			break;
		case OP_FCONV_TO_U4:
		case OP_FCONV_TO_U:
			code = emit_float_to_int (cfg, code, ins->dreg, ins->sreg1, 4, FALSE);
			break;
		case OP_SQRT:
			mips_fsqrtd (code, ins->dreg, ins->sreg1);
			break;
		case OP_FADD:
			mips_faddd (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;
		case OP_FSUB:
			mips_fsubd (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;		
		case OP_FMUL:
			mips_fmuld (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;		
		case OP_FDIV:
			mips_fdivd (code, ins->dreg, ins->sreg1, ins->sreg2);
			break;		
		case OP_FNEG:
			mips_fnegd (code, ins->dreg, ins->sreg1);
			break;		
		case OP_FCEQ:
			mips_fcmpd (code, MIPS_FPU_EQ, ins->sreg1, ins->sreg2);
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_fbtrue (code, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;
		case OP_FCLT:
			mips_fcmpd (code, MIPS_FPU_LT, ins->sreg1, ins->sreg2);
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_fbtrue (code, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;
		case OP_FCLT_UN:
			/* Less than, or Unordered */
			mips_fcmpd (code, MIPS_FPU_ULT, ins->sreg1, ins->sreg2);
			mips_addiu (code, ins->dreg, mips_zero, 1);
			mips_fbtrue (code, 2);
			mips_nop (code);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			break;
		case OP_FCGT:
			mips_fcmpd (code, MIPS_FPU_ULE, ins->sreg1, ins->sreg2);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			mips_fbtrue (code, 2);
			mips_nop (code);
			mips_addiu (code, ins->dreg, mips_zero, 1);
			break;
		case OP_FCGT_UN:
			/* Greater than, or Unordered */
			mips_fcmpd (code, MIPS_FPU_OLE, ins->sreg1, ins->sreg2);
			MIPS_MOVE (code, ins->dreg, mips_zero);
			mips_fbtrue (code, 2);
			mips_nop (code);
			mips_addiu (code, ins->dreg, mips_zero, 1);
			break;
		case OP_MIPS_FBEQ:
			mips_fcmpd (code, MIPS_FPU_EQ, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbtrue (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBNE:
			mips_fcmpd (code, MIPS_FPU_EQ, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbfalse (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBLT:
			mips_fcmpd (code, MIPS_FPU_LT, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbtrue (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBLT_UN:
			mips_fcmpd (code, MIPS_FPU_ULT, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbtrue (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBGT:
			mips_fcmpd (code, MIPS_FPU_LE, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbfalse (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBGT_UN:
			mips_fcmpd (code, MIPS_FPU_OLE, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbfalse (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBGE:
			mips_fcmpd (code, MIPS_FPU_LT, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbfalse (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBGE_UN:
			mips_fcmpd (code, MIPS_FPU_OLT, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbfalse (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBLE:
			mips_fcmpd (code, MIPS_FPU_OLE, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbtrue (code, 0);
			mips_nop (code);
			break;
		case OP_MIPS_FBLE_UN:
			mips_fcmpd (code, MIPS_FPU_ULE, ins->sreg1, ins->sreg2);
			mips_nop (code);
			mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_BB, ins->inst_true_bb);
			mips_fbtrue (code, 0);
			mips_nop (code);
			break;
		case OP_CKFINITE: {
			guint32 *branch_patch;

			mips_mfc1 (code, mips_at, ins->sreg1+1);
			mips_srl (code, mips_at, mips_at, 16+4);
			mips_andi (code, mips_at, mips_at, 2047);
			mips_addiu (code, mips_at, mips_at, -2047);

			branch_patch = (guint32 *)(void *)code;
			mips_bne (code, mips_at, mips_zero, 0);
			mips_nop (code);

			EMIT_SYSTEM_EXCEPTION_NAME("ArithmeticException");
			mips_patch (branch_patch, (guint32)code);
			mips_fmovd (code, ins->dreg, ins->sreg1);
			break;
		}
		case OP_JUMP_TABLE:
			mono_add_patch_info (cfg, offset, (MonoJumpInfoType)ins->inst_c1, ins->inst_p0);
			mips_load (code, ins->dreg, 0x0f0f0f0f);
			break;


		default:
			g_warning ("unknown opcode %s in %s()\n", mono_inst_name (ins->opcode), __FUNCTION__);
			g_assert_not_reached ();
		}

		if ((cfg->opt & MONO_OPT_BRANCH) && ((code - cfg->native_code - offset) > max_len)) {
			g_warning ("wrong maximal instruction length of instruction %s (expected %d, got %d)",
				   mono_inst_name (ins->opcode), max_len, code - cfg->native_code - offset);
			g_assert_not_reached ();
		}
	       
		cpos += max_len;

		last_ins = ins;
		last_offset = offset;
	}

	cfg->code_len = code - cfg->native_code;
}

void
mono_arch_register_lowlevel_calls (void)
{
}

void
mono_arch_patch_code (MonoMethod *method, MonoDomain *domain, guint8 *code, MonoJumpInfo *ji, gboolean run_cctors)
{
	MonoJumpInfo *patch_info;

	for (patch_info = ji; patch_info; patch_info = patch_info->next) {
		unsigned char *ip = patch_info->ip.i + code;
		const unsigned char *target = NULL;

		switch (patch_info->type) {
		case MONO_PATCH_INFO_IP:
			patch_lui_addiu ((guint32 *)(void *)ip, (guint32)ip);
			continue;
		case MONO_PATCH_INFO_SWITCH: {
			gpointer *table = (gpointer *)patch_info->data.table->table;
			int i;

			patch_lui_addiu ((guint32 *)(void *)ip, (guint32)table);

			for (i = 0; i < patch_info->data.table->table_size; i++) { 
				table [i] = (int)patch_info->data.table->table [i] + code;
			}
			continue;
		}
		case MONO_PATCH_INFO_METHODCONST:
		case MONO_PATCH_INFO_CLASS:
		case MONO_PATCH_INFO_IMAGE:
		case MONO_PATCH_INFO_FIELD:
		case MONO_PATCH_INFO_VTABLE:
		case MONO_PATCH_INFO_IID:
		case MONO_PATCH_INFO_SFLDA:
		case MONO_PATCH_INFO_LDSTR:
		case MONO_PATCH_INFO_TYPE_FROM_HANDLE:
		case MONO_PATCH_INFO_LDTOKEN:
		case MONO_PATCH_INFO_R4:
		case MONO_PATCH_INFO_R8:
			/* from OP_AOTCONST : lui + addiu */
			target = mono_resolve_patch_target (method, domain, code, patch_info, run_cctors);
			patch_lui_addiu ((guint32 *)(void *)ip, (guint32)target);
			continue;
#if 0
		case MONO_PATCH_INFO_EXC_NAME:
			g_assert_not_reached ();
			*((gconstpointer *)(void *)(ip + 1)) = patch_info->data.name;
			continue;
#endif
		case MONO_PATCH_INFO_NONE:
			/* everything is dealt with at epilog output time */
			continue;
		default:
			target = mono_resolve_patch_target (method, domain, code, patch_info, run_cctors);
			mips_patch ((guint32 *)(void *)ip, (guint32)target);
			break;
		}
	}
}

#if 0
static
void
mono_trace_lmf_prolog (MonoLMF *new_lmf)
{
}

static
void
mono_trace_lmf_epilog (MonoLMF *old_lmf)
{
}
#endif

/*
 * Allow tracing to work with this interface (with an optional argument)
 *
 * This code is expected to be inserted just after the 'real' prolog code,
 * and before the first basic block.  We need to allocate a 2nd, temporary
 * stack frame so that we can preserve f12-f15 as well as a0-a3.
 */

void*
mono_arch_instrument_prolog (MonoCompile *cfg, void *func, void *p, gboolean enable_arguments)
{
	guchar *code = p;
	int offset = cfg->arch.tracing_offset;

	mips_nop (code);
	mips_nop (code);
	mips_nop (code);

	/* For N32, need to know for each stack slot if it's an integer
	 * or float argument, and save/restore the appropriate register
	 */
	MIPS_SW (code, mips_a0, mips_sp, offset + 0*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a1, mips_sp, offset + 1*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a2, mips_sp, offset + 2*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a3, mips_sp, offset + 3*SIZEOF_REGISTER);
#if _MIPS_SIM == _ABIN32
	MIPS_SW (code, mips_a4, mips_sp, offset + 4*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a5, mips_sp, offset + 5*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a6, mips_sp, offset + 6*SIZEOF_REGISTER);
	MIPS_SW (code, mips_a7, mips_sp, offset + 7*SIZEOF_REGISTER);
#endif

	mips_load_const (code, mips_a0, cfg->method);
	mips_addiu (code, mips_a1, mips_sp, offset);
	mips_load_const (code, mips_t9, func);
	mips_jalr (code, mips_t9, mips_ra);
	mips_nop (code);

	MIPS_LW (code, mips_a0, mips_sp, offset + 0*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a1, mips_sp, offset + 1*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a2, mips_sp, offset + 2*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a3, mips_sp, offset + 3*SIZEOF_REGISTER);
#if _MIPS_SIM == _ABIN32
	MIPS_LW (code, mips_a4, mips_sp, offset + 4*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a5, mips_sp, offset + 5*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a6, mips_sp, offset + 6*SIZEOF_REGISTER);
	MIPS_LW (code, mips_a7, mips_sp, offset + 7*SIZEOF_REGISTER);
#endif

	mips_nop (code);
	mips_nop (code);
	mips_nop (code);
	return code;
}

void
mips_adjust_stackframe(MonoCompile *cfg)
{
	MonoBasicBlock *bb;
	int delta, threshold, i;
	MonoMethodSignature *sig;
	int ra_offset;

	if (cfg->stack_offset == cfg->arch.local_alloc_offset)
		return;

	/* adjust cfg->stack_offset for account for down-spilling */
	cfg->stack_offset += SIZEOF_REGISTER;

	/* re-align cfg->stack_offset if needed (due to var spilling) */
	cfg->stack_offset = (cfg->stack_offset + MIPS_STACK_ALIGNMENT - 1) & ~(MIPS_STACK_ALIGNMENT - 1);
	delta = cfg->stack_offset - cfg->arch.local_alloc_offset;
	if (cfg->verbose_level > 2) {
		g_print ("mips_adjust_stackframe:\n");
		g_print ("\tspillvars allocated 0x%x -> 0x%x\n", cfg->arch.local_alloc_offset, cfg->stack_offset);
	}
	threshold = cfg->arch.local_alloc_offset;
	ra_offset = cfg->stack_offset - sizeof(gpointer);
	if (cfg->verbose_level > 2) {
		g_print ("\tra_offset %d/0x%x delta %d/0x%x\n", ra_offset, ra_offset, delta, delta);
	}

	sig = mono_method_signature (cfg->method);
	if (sig && sig->ret && MONO_TYPE_ISSTRUCT (sig->ret)) {
		cfg->vret_addr->inst_offset += delta;
	}
	for (i = 0; i < sig->param_count + sig->hasthis; ++i) {
		MonoInst *inst = cfg->args [i];

		inst->inst_offset += delta;
	}

	/*
	 * loads and stores based off the frame reg that (used to) lie
	 * above the spill var area need to be increased by 'delta'
	 * to make room for the spill vars.
	 */
	/* Need to find loads and stores to adjust that
	 * are above where the spillvars were inserted, but
	 * which are not the spillvar references themselves.
	 *
	 * Idea - since all offsets from fp are positive, make
	 * spillvar offsets negative to begin with so we can spot
	 * them here.
	 */

#if 1
	for (bb = cfg->bb_entry; bb; bb = bb->next_bb) {
		int ins_cnt = 0;
		MonoInst *ins;

		if (cfg->verbose_level > 2) {
			g_print ("BASIC BLOCK %d:\n", bb->block_num);
		}
		MONO_BB_FOR_EACH_INS (bb, ins) {
			int adj_c0 = 0;
			int adj_imm = 0;

			if (cfg->verbose_level > 2) {
				mono_print_ins_index (ins_cnt, ins);
			}
			if (MONO_IS_LOAD_MEMBASE(ins) && (ins->inst_basereg == mips_fp))
				adj_c0 = 1;
			if (MONO_IS_STORE_MEMBASE(ins) && (ins->dreg == mips_fp))
				adj_c0 = 1;
			/* The following two catch FP spills */
			if (MONO_IS_LOAD_MEMBASE(ins) && (ins->inst_basereg == mips_sp))
				adj_c0 = 1;
			if (MONO_IS_STORE_MEMBASE(ins) && (ins->dreg == mips_sp))
				adj_c0 = 1;
			if (((ins->opcode == OP_ADD_IMM) || (ins->opcode == OP_IADD_IMM)) && (ins->sreg1 == mips_fp))
				adj_imm = 1;
			if (adj_c0) {
				if (ins->inst_c0 >= threshold) {
					ins->inst_c0 += delta;
					if (cfg->verbose_level > 2) {
						g_print ("adj");
						mono_print_ins_index (ins_cnt, ins);
					}
				}
				else if (ins->inst_c0 < 0) {
					ins->inst_c0 = - ins->inst_c0 - 4;
					if (cfg->verbose_level > 2) {
						g_print ("spill");
						mono_print_ins_index (ins_cnt, ins);
					}
				}
				g_assert (ins->inst_c0 != ra_offset);
			}
			if (adj_imm) {
				if (ins->inst_imm >= threshold) {
					ins->inst_imm += delta;
					if (cfg->verbose_level > 2) {
						g_print ("adj");
						mono_print_ins_index (ins_cnt, ins);
					}
				}
				g_assert (ins->inst_c0 != ra_offset);
			}

			++ins_cnt;
		}
	}
#endif
}

/*
 * Stack frame layout:
 * 
 *   ------------------- sp + cfg->stack_usage + cfg->param_area
 *      param area		incoming
 *   ------------------- sp + cfg->stack_usage + MIPS_STACK_PARAM_OFFSET
 *      a0-a3			incoming
 *   ------------------- sp + cfg->stack_usage
 *	ra
 *   ------------------- sp + cfg->stack_usage-4
 *   	spilled regs
 *   ------------------- sp + 
 *   	MonoLMF structure	optional
 *   ------------------- sp + cfg->arch.lmf_offset
 *   	saved registers		s0-s8
 *   ------------------- sp + cfg->arch.iregs_offset
 *   	locals
 *   ------------------- sp + cfg->param_area
 *   	param area		outgoing
 *   ------------------- sp + MIPS_STACK_PARAM_OFFSET
 *   	a0-a3			outgoing
 *   ------------------- sp
 *   	red zone
 */
guint8 *
mono_arch_emit_prolog (MonoCompile *cfg)
{
	MonoMethod *method = cfg->method;
	MonoMethodSignature *sig;
	MonoInst *inst;
	int alloc_size, pos, i;
	guint8 *code;
	CallInfo *cinfo;
	int tracing = 0;
	guint32 iregs_to_save = 0;
#if SAVE_FP_REGS
	guint32 fregs_to_save = 0;
#endif
#if SAVE_LMF
	/* lmf_offset is the offset of the LMF from our stack pointer. */
	guint32 lmf_offset = cfg->arch.lmf_offset;
#endif

	if (mono_jit_trace_calls != NULL && mono_trace_eval (method))
		tracing = 1;

	if (tracing)
		cfg->flags |= MONO_CFG_HAS_CALLS;
	
	sig = mono_method_signature (method);
	cfg->code_size = 768 + sig->param_count * 20;
	code = cfg->native_code = g_malloc (cfg->code_size);

	if (tracing) {
#if _MIPS_SIM == _ABIO32
		cfg->arch.tracing_offset = cfg->stack_offset;
#elif _MIPS_SIM == _ABIN32
		/* no stack slots by default for argument regs, reserve a special block */
		cfg->arch.tracing_offset = cfg->stack_offset;
		cfg->stack_offset += 8 * SIZEOF_REGISTER;
#endif
	}

	/* adjust stackframe assignments for spillvars if needed */
	mips_adjust_stackframe (cfg);

	/* stack_offset should not be changed here. */
	alloc_size = cfg->stack_offset;
	cfg->stack_usage = alloc_size;

#if SAVE_ALL_REGS
	iregs_to_save = MONO_ARCH_CALLEE_SAVED_REGS;
#else
	iregs_to_save = (cfg->used_int_regs & MONO_ARCH_CALLEE_SAVED_REGS);
#endif
#if SAVE_FP_REGS
#if 0
	fregs_to_save = (cfg->used_float_regs & MONO_ARCH_CALLEE_SAVED_FREGS);
#else
	fregs_to_save = MONO_ARCH_CALLEE_SAVED_FREGS;
	fregs_to_save |= (fregs_to_save << 1);
#endif
#endif
	if (alloc_size) {
		if (mips_is_imm16 (-alloc_size)) {
			mips_addiu (code, mips_sp, mips_sp, -alloc_size);
		} else {
			mips_load_const (code, mips_at, -alloc_size);
			mips_addu (code, mips_sp, mips_sp, mips_at);
		}
	}

	if ((cfg->flags & MONO_CFG_HAS_CALLS) || ALWAYS_SAVE_RA)
		mips_sw (code, mips_ra, mips_sp, alloc_size + MIPS_RET_ADDR_OFFSET);

	/* XXX - optimize this later to not save all regs if LMF constructed */

	if (iregs_to_save) {
		/* save used registers in own stack frame (at pos) */
		pos = cfg->arch.iregs_offset;
		for (i = MONO_MAX_IREGS-1; i >= 0; --i) {
			if (iregs_to_save & (1 << i)) {
				g_assert (pos < cfg->stack_usage - sizeof(gpointer));
				MIPS_SW (code, i, mips_sp, pos);
				pos += SIZEOF_REGISTER;
			}
		}
	}
#if SAVE_LMF
	if (method->save_lmf) {
		for (i = MONO_MAX_IREGS-1; i >= 0; --i) {
			MIPS_SW (code, i, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, iregs[i]));
		}
	}
#endif

#if SAVE_FP_REGS
	/* Save float registers */
	if (fregs_to_save) {
		for (i = MONO_MAX_FREGS-1; i >= 0; --i) {
			if (fregs_to_save & (1 << i)) {
				g_assert (pos < cfg->stack_usage - MIPS_STACK_ALIGNMENT);
				mips_swc1 (code, i, mips_sp, pos);
				pos += sizeof (gulong);
			}
		}
	}
#if SAVE_LMF
	if (method->save_lmf) {
		for (i = MONO_MAX_FREGS-1; i >= 0; --i) {
			mips_swc1 (code, i, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, fregs[i]));
		}
	}
#endif
#endif
	if (cfg->frame_reg != mips_sp) {
		MIPS_MOVE (code, cfg->frame_reg, mips_sp);
#if SAVE_LMF
		if (method->save_lmf)
			MIPS_SW (code, cfg->frame_reg, mips_sp,
				 lmf_offset + G_STRUCT_OFFSET(MonoLMF, iregs[cfg->frame_reg]));
#endif
	}

	/* Do instrumentation before assigning regvars to registers.  Because they may be assigned
	 * to the t* registers, which would be clobbered by the instrumentation calls.
	 */
	if (tracing) {
		code = mono_arch_instrument_prolog (cfg, mono_trace_enter_method, code, TRUE);
	}


	/* load arguments allocated to register from the stack */
	pos = 0;

	cinfo = calculate_sizes (sig, sig->pinvoke);

	if (MONO_TYPE_ISSTRUCT (sig->ret)) {
		ArgInfo *ainfo = &cinfo->ret;
		inst = cfg->vret_addr;
		if (inst->opcode == OP_REGVAR)
			MIPS_MOVE (code, inst->dreg, ainfo->reg);
		else if (mips_is_imm16 (inst->inst_offset)) {
			mips_sw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
		} else {
			mips_load_const (code, mips_at, inst->inst_offset);
			mips_addu (code, mips_at, mips_at, inst->inst_basereg);
			mips_sw (code, ainfo->reg, mips_at, 0);
		}
	}
	/* Keep this in sync with emit_load_volatile_arguments */
	for (i = 0; i < sig->param_count + sig->hasthis; ++i) {
		ArgInfo *ainfo = cinfo->args + i;
		inst = cfg->args [pos];
		
		if (cfg->verbose_level > 2)
			g_print ("Saving argument %d (type: %d)\n", i, ainfo->regtype);
		if (inst->opcode == OP_REGVAR) {
			/* Argument ends up in a register */
			if (ainfo->regtype == RegTypeGeneral)
				MIPS_MOVE (code, inst->dreg, ainfo->reg);
			else if (ainfo->regtype == RegTypeFP) {
				g_assert_not_reached();
#if 0
				ppc_fmr (code, inst->dreg, ainfo->reg);
#endif
			}
			else if (ainfo->regtype == RegTypeBase) {
				mips_lw (code, inst->dreg, mips_sp, cfg->stack_usage + ainfo->offset);
			} else
				g_assert_not_reached ();

			if (cfg->verbose_level > 2)
				g_print ("Argument %d assigned to register %s\n", pos, mono_arch_regname (inst->dreg));
		} else {
			/* Argument ends up on the stack */
			if (ainfo->regtype == RegTypeGeneral) {
				/* Incoming parameters should be above this frame */
				if (cfg->verbose_level > 2)
					g_print ("stack slot at %d of %d\n", inst->inst_offset, alloc_size);
				/* g_assert (inst->inst_offset >= alloc_size); */
				g_assert (mips_is_imm16 (inst->inst_offset));
				switch (ainfo->size) {
				case 1:
					mips_sb (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 2:
					mips_sh (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 0: /* XXX */
				case 4:
					mips_sw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					break;
				case 8:
#if (SIZEOF_REGISTER == 4)
					mips_sw (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
					mips_sw (code, ainfo->reg + 1, inst->inst_basereg, inst->inst_offset + 4);
#elif (SIZEOF_REGISTER == 8)
					mips_sd (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
#endif
					break;
				default:
					g_assert_not_reached ();
					break;
				}
			} else if (ainfo->regtype == RegTypeBase) {
				/*
				 * Argument comes in on the stack, and ends up on the stack
				 * 1 and 2 byte args are passed as 32-bit quantities, but used as
				 * 8 and 16 bit quantities.  Shorten them in place.
				 */
				g_assert (mips_is_imm16 (inst->inst_offset));
				switch (ainfo->size) {
				case 1:
					mips_lw (code, mips_at, inst->inst_basereg, inst->inst_offset);
					mips_sb (code, mips_at, inst->inst_basereg, inst->inst_offset);
					break;
				case 2:
					mips_lw (code, mips_at, inst->inst_basereg, inst->inst_offset);
					mips_sh (code, mips_at, inst->inst_basereg, inst->inst_offset);
					break;
				case 0: /* XXX */
				case 4:
				case 8:
					break;
				default:
					g_assert_not_reached ();
				}
			} else if (ainfo->regtype == RegTypeFP) {
				g_assert (mips_is_imm16 (inst->inst_offset));
				if (ainfo->size == 8) {
#if _MIPS_SIM == _ABIO32
					mips_swc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset+4);
					mips_swc1 (code, ainfo->reg+1, inst->inst_basereg, inst->inst_offset);
#elif _MIPS_SIM == _ABIN32
					mips_sdc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
#endif
				}
				else if (ainfo->size == 4)
					mips_swc1 (code, ainfo->reg, inst->inst_basereg, inst->inst_offset);
				else
					g_assert_not_reached ();
			} else if (ainfo->regtype == RegTypeStructByVal) {
				int i;
				int doffset = inst->inst_offset;

				g_assert (mips_is_imm16 (inst->inst_offset));
				g_assert (mips_is_imm16 (inst->inst_offset + ainfo->size * sizeof (gpointer)));
				/* Push the argument registers into their stack slots */
				for (i = 0; i < ainfo->size; ++i) {
					MIPS_SW (code, ainfo->reg + i, inst->inst_basereg, doffset);
					doffset += SIZEOF_REGISTER;
				}
			} else if (ainfo->regtype == RegTypeStructByAddr) {
				g_assert (mips_is_imm16 (inst->inst_offset));
				/* FIXME: handle overrun! with struct sizes not multiple of 4 */
				code = emit_memcpy (code, ainfo->vtsize * sizeof (gpointer), inst->inst_basereg, inst->inst_offset, ainfo->reg, 0);
			} else
				g_assert_not_reached ();
		}
		pos++;
	}

	if (method->wrapper_type == MONO_WRAPPER_NATIVE_TO_MANAGED) {
		mips_load_const (code, mips_a0, cfg->domain);
		mips_load_const (code, mips_t9, (gpointer)mono_jit_thread_attach);
		mips_jalr (code, mips_t9, mips_ra);
		mips_nop (code);
	}

#if SAVE_LMF
	if (method->save_lmf) {
		mips_load_const (code, mips_at, MIPS_LMF_MAGIC1);
		mips_sw (code, mips_at, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, magic));

		if (lmf_pthread_key != -1) {
			g_assert_not_reached();
#if 0
			emit_tls_access (code, mips_temp, lmf_pthread_key);
#endif
			if (G_STRUCT_OFFSET (MonoJitTlsData, lmf))
				mips_addiu (code, mips_a0, mips_temp, G_STRUCT_OFFSET (MonoJitTlsData, lmf));
		} else {
#if 0
			mips_addiu (code, mips_a0, mips_sp, lmf_offset);
			mips_load_const (code, mips_t9, (gpointer)mono_trace_lmf_prolog);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
#endif
			/* This can/will clobber the a0-a3 registers */
			mips_load_const (code, mips_t9, (gpointer)mono_get_lmf_addr);
			mips_jalr (code, mips_t9, mips_ra);
			mips_nop (code);
		}

		/* mips_v0 is the result from mono_get_lmf_addr () (MonoLMF **) */
		mips_sw (code, mips_v0, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, lmf_addr));
		/* new_lmf->previous_lmf = *lmf_addr */
		mips_lw (code, mips_at, mips_v0, 0);
		mips_sw (code, mips_at, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, previous_lmf));
		/* *(lmf_addr) = sp + lmf_offset */
		mips_addiu (code, mips_at, mips_sp, lmf_offset);
		mips_sw (code, mips_at, mips_v0, 0);

		/* save method info */
		mips_load_const (code, mips_at, method);
		mips_sw (code, mips_at, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, method));
		MIPS_SW (code, mips_sp, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, ebp));

		/* save the current IP */
		mono_add_patch_info (cfg, code - cfg->native_code, MONO_PATCH_INFO_IP, NULL);
		mips_load_const (code, mips_at, 0x01010101);
		mips_sw (code, mips_at, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, eip));
	}
#endif

	cfg->code_len = code - cfg->native_code;
	g_assert (cfg->code_len < cfg->code_size);
	g_free (cinfo);

	return code;
}

enum {
	SAVE_NONE,
	SAVE_STRUCT,
	SAVE_ONE,
	SAVE_TWO,
	SAVE_FP
};

void*
mono_arch_instrument_epilog_full (MonoCompile *cfg, void *func, void *p, gboolean enable_arguments, gboolean preserve_argument_registers)
{
	guchar *code = p;
	int save_mode = SAVE_NONE;
	int offset;
	MonoMethod *method = cfg->method;
	int rtype = mono_type_get_underlying_type (mono_method_signature (method)->ret)->type;
	int save_offset = MIPS_STACK_PARAM_OFFSET;

	g_assert ((save_offset & (MIPS_STACK_ALIGNMENT-1)) == 0);
	
	offset = code - cfg->native_code;
	/* we need about 16 instructions */
	if (offset > (cfg->code_size - 16 * 4)) {
		cfg->code_size *= 2;
		cfg->native_code = g_realloc (cfg->native_code, cfg->code_size);
		code = cfg->native_code + offset;
	}
	mips_nop (code);
	mips_nop (code);
	switch (rtype) {
	case MONO_TYPE_VOID:
		/* special case string .ctor icall */
		if (strcmp (".ctor", method->name) && method->klass == mono_defaults.string_class)
			save_mode = SAVE_ONE;
		else
			save_mode = SAVE_NONE;
		break;
	case MONO_TYPE_R4:
	case MONO_TYPE_R8:
		save_mode = SAVE_FP;
		break;
	case MONO_TYPE_VALUETYPE:
		save_mode = SAVE_STRUCT;
		break;
	case MONO_TYPE_I8:
	case MONO_TYPE_U8:
#if SIZEOF_REGISTER == 4
		save_mode = SAVE_TWO;
#elif SIZEOF_REGISTER == 8
		save_mode = SAVE_ONE;
#endif
		break;
	default:
		save_mode = SAVE_ONE;
		break;
	}

	mips_addiu (code, mips_sp, mips_sp, -32);
	switch (save_mode) {
	case SAVE_TWO:
		mips_sw (code, mips_v0, mips_sp, save_offset);
		mips_sw (code, mips_v1, mips_sp, save_offset + SIZEOF_REGISTER);
		if (enable_arguments) {
			MIPS_MOVE (code, mips_a1, mips_v0);
			MIPS_MOVE (code, mips_a2, mips_v1);
		}
		break;
	case SAVE_ONE:
		MIPS_SW (code, mips_v0, mips_sp, save_offset);
		if (enable_arguments) {
			MIPS_MOVE (code, mips_a1, mips_v0);
		}
		break;
	case SAVE_FP:
		mips_sdc1 (code, mips_f0, mips_sp, save_offset);
		mips_ldc1 (code, mips_f12, mips_sp, save_offset);
		mips_lw (code, mips_a0, mips_sp, save_offset);
		mips_lw (code, mips_a1, mips_sp, save_offset + SIZEOF_REGISTER);
		break;
	case SAVE_STRUCT:
	case SAVE_NONE:
	default:
		break;
	}
	mips_load_const (code, mips_a0, cfg->method);
	mips_load_const (code, mips_t9, func);
	mips_jalr (code, mips_t9, mips_ra);
	mips_nop (code);

	switch (save_mode) {
	case SAVE_TWO:
		mips_lw (code, mips_v0, mips_sp, save_offset);
		mips_lw (code, mips_v1, mips_sp, save_offset + SIZEOF_REGISTER);
		break;
	case SAVE_ONE:
		MIPS_LW (code, mips_v0, mips_sp, save_offset);
		break;
	case SAVE_FP:
		mips_ldc1 (code, mips_f0, mips_sp, save_offset);
		break;
	case SAVE_STRUCT:
	case SAVE_NONE:
	default:
		break;
	}
	mips_addiu (code, mips_sp, mips_sp, 32);
	mips_nop (code);
	mips_nop (code);
	return code;
}

guint8 *
mono_arch_emit_epilog_sub (MonoCompile *cfg, guint8 *code)
{
	MonoMethod *method = cfg->method;
	int pos = 0, i;
	int max_epilog_size = 16 + 20*4;
	guint32 iregs_to_restore;
#if SAVE_FP_REGS
	guint32 fregs_to_restore;
#endif

#if SAVE_LMF
	if (cfg->method->save_lmf)
		max_epilog_size += 128;
#endif
	
	if (mono_jit_trace_calls != NULL)
		max_epilog_size += 50;

	if (cfg->prof_options & MONO_PROFILE_ENTER_LEAVE)
		max_epilog_size += 50;

	if (code)
		pos = code - cfg->native_code;
	while (cfg->code_len + max_epilog_size > (cfg->code_size - 16)) {
		cfg->code_size *= 2;
		cfg->native_code = g_realloc (cfg->native_code, cfg->code_size);
		mono_jit_stats.code_reallocs++;
	}

	/*
	 * Keep in sync with OP_JMP
	 */
	if (code)
		code = cfg->native_code + pos;
	else
		code = cfg->native_code + cfg->code_len;

	if (mono_jit_trace_calls != NULL && mono_trace_eval (method)) {
		code = mono_arch_instrument_epilog (cfg, mono_trace_leave_method, code, TRUE);
	}
	pos = cfg->arch.iregs_offset;
	if (cfg->frame_reg != mips_sp) {
		MIPS_MOVE (code, mips_sp, cfg->frame_reg);
	}
#if SAVE_ALL_REGS
	iregs_to_restore = MONO_ARCH_CALLEE_SAVED_REGS;
#else
	iregs_to_restore = (cfg->used_int_regs & MONO_ARCH_CALLEE_SAVED_REGS);
#endif
	if (iregs_to_restore) {
		for (i = MONO_MAX_IREGS-1; i >= 0; --i) {
			if (iregs_to_restore & (1 << i)) {
				MIPS_LW (code, i, mips_sp, pos);
				pos += SIZEOF_REGISTER;
			}
		}
	}

#if SAVE_FP_REGS
#if 0
	fregs_to_restore = (cfg->used_float_regs & MONO_ARCH_CALLEE_SAVED_FREGS);
#else
	fregs_to_restore = MONO_ARCH_CALLEE_SAVED_FREGS;
	fregs_to_restore |= (fregs_to_restore << 1);
#endif
	if (fregs_to_restore) {
		for (i = MONO_MAX_FREGS-1; i >= 0; --i) {
			if (fregs_to_restore & (1 << i)) {
				g_assert (pos < cfg->stack_usage - MIPS_STACK_ALIGNMENT);
				mips_lwc1 (code, i, mips_sp, pos);
				pos += FREG_SIZE
			}
		}
	}
#endif
#if SAVE_LMF
	/* Unlink the LMF if necessary */
	if (method->save_lmf) {
		int lmf_offset = cfg->arch.lmf_offset;

		/* t0 = current_lmf->previous_lmf */
		mips_lw (code, mips_temp, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, previous_lmf));
		/* t1 = lmf_addr */
		mips_lw (code, mips_t1, mips_sp, lmf_offset + G_STRUCT_OFFSET(MonoLMF, lmf_addr));
		/* (*lmf_addr) = previous_lmf */
		mips_sw (code, mips_temp, mips_t1, 0);
	}
#endif
#if 0
	/* Restore the fp */
	mips_lw (code, mips_fp, mips_sp, cfg->stack_usage + MIPS_FP_ADDR_OFFSET);
#endif
	/* Correct the stack pointer */
	if ((cfg->flags & MONO_CFG_HAS_CALLS) || ALWAYS_SAVE_RA)
		mips_lw (code, mips_ra, mips_sp, cfg->stack_usage + MIPS_RET_ADDR_OFFSET);
	mips_addiu (code, mips_sp, mips_sp, cfg->stack_usage);

	/* Caller will emit either return or tail-call sequence */

	cfg->code_len = code - cfg->native_code;

	g_assert (cfg->code_len < cfg->code_size);
	return (code);
}

void
mono_arch_emit_epilog (MonoCompile *cfg)
{
	guint8 *code;

	code = mono_arch_emit_epilog_sub (cfg, NULL);

	mips_jr (code, mips_ra);
	mips_nop (code);

	cfg->code_len = code - cfg->native_code;

	g_assert (cfg->code_len < cfg->code_size);
}

/* remove once throw_exception_by_name is eliminated */
#if 0
static int
exception_id_by_name (const char *name)
{
	if (strcmp (name, "IndexOutOfRangeException") == 0)
		return MONO_EXC_INDEX_OUT_OF_RANGE;
	if (strcmp (name, "OverflowException") == 0)
		return MONO_EXC_OVERFLOW;
	if (strcmp (name, "ArithmeticException") == 0)
		return MONO_EXC_ARITHMETIC;
	if (strcmp (name, "DivideByZeroException") == 0)
		return MONO_EXC_DIVIDE_BY_ZERO;
	if (strcmp (name, "InvalidCastException") == 0)
		return MONO_EXC_INVALID_CAST;
	if (strcmp (name, "NullReferenceException") == 0)
		return MONO_EXC_NULL_REF;
	if (strcmp (name, "ArrayTypeMismatchException") == 0)
		return MONO_EXC_ARRAY_TYPE_MISMATCH;
	g_error ("Unknown intrinsic exception %s\n", name);
	return 0;
}
#endif

void
mono_arch_emit_exceptions (MonoCompile *cfg)
{
#if 0
	MonoJumpInfo *patch_info;
	int i;
	guint8 *code;
	const guint8* exc_throw_pos [MONO_EXC_INTRINS_NUM] = {NULL};
	guint8 exc_throw_found [MONO_EXC_INTRINS_NUM] = {0};
	int max_epilog_size = 50;

	/* count the number of exception infos */
     
	/* 
	 * make sure we have enough space for exceptions
	 * 24 is the simulated call to throw_exception_by_name
	 */
	for (patch_info = cfg->patch_info; patch_info; patch_info = patch_info->next) {
#if 0
		if (patch_info->type == MONO_PATCH_INFO_EXC) {
			i = exception_id_by_name (patch_info->data.target);
			g_assert (i < MONO_EXC_INTRINS_NUM);
			if (!exc_throw_found [i]) {
				max_epilog_size += 12;
				exc_throw_found [i] = TRUE;
			}
		}
#endif
	}

	while (cfg->code_len + max_epilog_size > (cfg->code_size - 16)) {
		cfg->code_size *= 2;
		cfg->native_code = g_realloc (cfg->native_code, cfg->code_size);
		mono_jit_stats.code_reallocs++;
	}

	code = cfg->native_code + cfg->code_len;

	/* add code to raise exceptions */
	for (patch_info = cfg->patch_info; patch_info; patch_info = patch_info->next) {
		switch (patch_info->type) {
		case MONO_PATCH_INFO_EXC: {
#if 0
			//unsigned char *ip = patch_info->ip.i + cfg->native_code;

			i = exception_id_by_name (patch_info->data.target);
			g_assert (i >= 0 && i < MONO_EXC_INTRINS_NUM);
			if (!exc_throw_pos [i]) {
				guint32 addr;

				exc_throw_pos [i] = code;
				//g_print ("exc: writing stub at %p\n", code);
				mips_load_const (code, mips_a0, patch_info->data.target);
				addr = (guint32) mono_arch_get_throw_exception_by_name ();
				mips_load_const (code, mips_t9, addr);
				mips_jr (code, mips_t9);
				mips_nop (code);
			}
			//g_print ("exc: patch %p to %p\n", ip, exc_throw_pos[i]);

			/* Turn into a Relative patch, pointing at code stub */
			patch_info->type = MONO_PATCH_INFO_METHOD_REL;
			patch_info->data.offset = exc_throw_pos[i] - cfg->native_code;
#else
			g_assert_not_reached();
#endif
			break;
		}
		default:
			/* do nothing */
			break;
		}
	}

	cfg->code_len = code - cfg->native_code;

	g_assert (cfg->code_len < cfg->code_size);
#endif
}

/*
 * Thread local storage support
 */
static void
setup_tls_access (void)
{
	guint32 ptk;
	//guint32 *ins, *code;

	if (tls_mode == TLS_MODE_FAILED)
		return;

	if (g_getenv ("MONO_NO_TLS")) {
		tls_mode = TLS_MODE_FAILED;
		return;
	}

	if (tls_mode == TLS_MODE_DETECT) {
		/* XXX */
		tls_mode = TLS_MODE_FAILED;
		return;
#if 0

		ins = (guint32*)pthread_getspecific;
		/* uncond branch to the real method */
		if ((*ins >> 26) == 18) {
			gint32 val;
			val = (*ins & ~3) << 6;
			val >>= 6;
			if (*ins & 2) {
				/* absolute */
				ins = (guint32*)val;
			} else {
				ins = (guint32*) ((char*)ins + val);
			}
		}
		code = &cmplwi_1023;
		ppc_cmpli (code, 0, 0, ppc_r3, 1023);
		code = &li_0x48;
		ppc_li (code, ppc_r4, 0x48);
		code = &blr_ins;
		ppc_blr (code);
		if (*ins == cmplwi_1023) {
			int found_lwz_284 = 0;
			for (ptk = 0; ptk < 20; ++ptk) {
				++ins;
				if (!*ins || *ins == blr_ins)
					break;
				if ((guint16)*ins == 284 && (*ins >> 26) == 32) {
					found_lwz_284 = 1;
					break;
				}
			}
			if (!found_lwz_284) {
				tls_mode = TLS_MODE_FAILED;
				return;
			}
			tls_mode = TLS_MODE_LTHREADS;
		} else if (*ins == li_0x48) {
			++ins;
			/* uncond branch to the real method */
			if ((*ins >> 26) == 18) {
				gint32 val;
				val = (*ins & ~3) << 6;
				val >>= 6;
				if (*ins & 2) {
					/* absolute */
					ins = (guint32*)val;
				} else {
					ins = (guint32*) ((char*)ins + val);
				}
				code = &val;
				ppc_li (code, ppc_r0, 0x7FF2);
				if (ins [1] == val) {
					/* Darwin on G4, implement */
					tls_mode = TLS_MODE_FAILED;
					return;
				} else {
					code = &val;
					ppc_mfspr (code, ppc_r3, 104);
					if (ins [1] != val) {
						tls_mode = TLS_MODE_FAILED;
						return;
					}
					tls_mode = TLS_MODE_DARWIN_G5;
				}
			} else {
				tls_mode = TLS_MODE_FAILED;
				return;
			}
		} else {
			tls_mode = TLS_MODE_FAILED;
			return;
		}
#endif
	}
	if (monodomain_key == -1) {
		ptk = mono_domain_get_tls_key ();
		if (ptk < 1024) {
			ptk = mono_pthread_key_for_tls (ptk);
			if (ptk < 1024) {
				monodomain_key = ptk;
			}
		}
	}
	if (lmf_pthread_key == -1) {
		ptk = mono_pthread_key_for_tls (mono_jit_tls_id);
		if (ptk < 1024) {
			/*g_print ("MonoLMF at: %d\n", ptk);*/
			/*if (!try_offset_access (mono_get_lmf_addr (), ptk)) {
				init_tls_failed = 1;
				return;
			}*/
			lmf_pthread_key = ptk;
		}
	}
	if (monothread_key == -1) {
		ptk = mono_thread_get_tls_key ();
		if (ptk < 1024) {
			ptk = mono_pthread_key_for_tls (ptk);
			if (ptk < 1024) {
				monothread_key = ptk;
				/*g_print ("thread inited: %d\n", ptk);*/
			}
		} else {
			/*g_print ("thread not inited yet %d\n", ptk);*/
		}
	}
}

void
mono_arch_setup_jit_tls_data (MonoJitTlsData *tls)
{
	setup_tls_access ();
}

void
mono_arch_free_jit_tls_data (MonoJitTlsData *tls)
{
}

void
mono_arch_emit_this_vret_args (MonoCompile *cfg, MonoCallInst *inst, int this_reg, int this_type, int vt_reg)
{
	int this_dreg = mips_a0;
	
	if (vt_reg != -1)
		this_dreg = mips_a1;

	/* add the this argument */
	if (this_reg != -1) {
		MonoInst *this;
		MONO_INST_NEW (cfg, this, OP_MOVE);
		this->type = this_type;
		this->sreg1 = this_reg;
		this->dreg = mono_alloc_ireg (cfg);
		mono_bblock_add_inst (cfg->cbb, this);
		mono_call_inst_add_outarg_reg (cfg, inst, this->dreg, this_dreg, FALSE);
	}

	if (vt_reg != -1) {
		MonoInst *vtarg;
		MONO_INST_NEW (cfg, vtarg, OP_MOVE);
		vtarg->type = STACK_MP;
		vtarg->sreg1 = vt_reg;
		vtarg->dreg = mono_alloc_ireg (cfg);
		mono_bblock_add_inst (cfg->cbb, vtarg);
		mono_call_inst_add_outarg_reg (cfg, inst, vtarg->dreg, mips_a0, FALSE);
	}
}

MonoInst*
mono_arch_get_inst_for_method (MonoCompile *cfg, MonoMethod *cmethod, MonoMethodSignature *fsig, MonoInst **args)
{
	MonoInst *ins = NULL;

	return ins;
}

MonoInst*
mono_arch_emit_inst_for_method (MonoCompile *cfg, MonoMethod *cmethod, MonoMethodSignature *fsig, MonoInst **args)
{
	return NULL;
}

gboolean
mono_arch_print_tree (MonoInst *tree, int arity)
{
	return 0;
}

MonoInst* mono_arch_get_domain_intrinsic (MonoCompile* cfg)
{
	MonoInst* ins;

	setup_tls_access ();
	if (monodomain_key == -1)
		return NULL;
	
	MONO_INST_NEW (cfg, ins, OP_TLS_GET);
	ins->inst_offset = monodomain_key;
	return ins;
}

MonoInst* 
mono_arch_get_thread_intrinsic (MonoCompile* cfg)
{
	MonoInst* ins;

	setup_tls_access ();
	if (monothread_key == -1)
		return NULL;
	
	MONO_INST_NEW (cfg, ins, OP_TLS_GET);
	ins->inst_offset = monothread_key;
	return ins;
}

gpointer
mono_arch_context_get_int_reg (MonoContext *ctx, int reg)
{
	/* FIXME: implement */
	g_assert_not_reached ();
}

#ifdef MONO_ARCH_HAVE_IMT

#define CMP_SIZE 12
#define BR_SIZE 4
#define JUMP_IMM_SIZE 12
#define JUMP_IMM32_SIZE 16
#define ENABLE_WRONG_METHOD_CHECK 0

/*
 * LOCKING: called with the domain lock held
 */
gpointer
mono_arch_build_imt_thunk (MonoVTable *vtable, MonoDomain *domain, MonoIMTCheckItem **imt_entries, int count,
	gpointer fail_tramp)
{
	NOT_IMPLEMENTED;
#if 0
	int i;
	int size = 0;
	guint8 *code, *start;

	for (i = 0; i < count; ++i) {
		MonoIMTCheckItem *item = imt_entries [i];
		if (item->is_equals) {
			if (item->check_target_idx) {
				if (!item->compare_done)
					item->chunk_size += CMP_SIZE;
				if (fail_tramp)
					item->chunk_size += BR_SIZE + JUMP_IMM32_SIZE;
				else
					item->chunk_size += BR_SIZE + JUMP_IMM_SIZE;
			} else {
				if (fail_tramp) {
					item->chunk_size += CMP_SIZE + BR_SIZE + JUMP_IMM32_SIZE * 2;
				} else {
					item->chunk_size += JUMP_IMM_SIZE;
#if ENABLE_WRONG_METHOD_CHECK
					item->chunk_size += CMP_SIZE + BR_SIZE + 4;
#endif
				}
			}
		} else {
			item->chunk_size += CMP_SIZE + BR_SIZE;
			imt_entries [item->check_target_idx]->compare_done = TRUE;
		}
		size += item->chunk_size;
	}
	if (fail_tramp) {
		code = mono_method_alloc_generic_virtual_thunk (domain, size);
	} else {
		/* the initial load of the vtable address */
		size += 8;
		code = mono_domain_code_reserve (domain, size);
	}
	start = code;
	if (!fail_tramp)
		ppc_load (code, ppc_r11, (guint32)(& (vtable->vtable [0])));
	for (i = 0; i < count; ++i) {
		MonoIMTCheckItem *item = imt_entries [i];
		item->code_target = code;
		if (item->is_equals) {
			if (item->check_target_idx) {
				if (!item->compare_done) {
					ppc_load (code, ppc_r0, (guint32)item->key);
					ppc_cmpl (code, 0, 0, MONO_ARCH_IMT_REG, ppc_r0);
				}
				item->jmp_code = code;
				ppc_bc (code, PPC_BR_FALSE, PPC_BR_EQ, 0);
				if (fail_tramp)
					ppc_load (code, ppc_r0, item->value.target_code);
				else
					ppc_lwz (code, ppc_r0, (sizeof (gpointer) * item->value.vtable_slot), ppc_r11);
				ppc_mtctr (code, ppc_r0);
				ppc_bcctr (code, PPC_BR_ALWAYS, 0);
			} else {
				if (fail_tramp) {
					ppc_load (code, ppc_r0, (guint32)item->key);
					ppc_cmpl (code, 0, 0, MONO_ARCH_IMT_REG, ppc_r0);
					item->jmp_code = code;
					ppc_bc (code, PPC_BR_FALSE, PPC_BR_EQ, 0);
					ppc_load (code, ppc_r0, item->value.target_code);
					ppc_mtctr (code, ppc_r0);
					ppc_bcctr (code, PPC_BR_ALWAYS, 0);
					ppc_patch (item->jmp_code, code);
					ppc_load (code, ppc_r0, fail_tramp);
					ppc_mtctr (code, ppc_r0);
					ppc_bcctr (code, PPC_BR_ALWAYS, 0);
					item->jmp_code = NULL;
				} else {
					/* enable the commented code to assert on wrong method */
#if ENABLE_WRONG_METHOD_CHECK
					ppc_load (code, ppc_r0, (guint32)item->key);
					ppc_cmpl (code, 0, 0, MONO_ARCH_IMT_REG, ppc_r0);
					item->jmp_code = code;
					ppc_bc (code, PPC_BR_FALSE, PPC_BR_EQ, 0);
#endif
					ppc_lwz (code, ppc_r0, (sizeof (gpointer) * item->value.vtable_slot), ppc_r11);
					ppc_mtctr (code, ppc_r0);
					ppc_bcctr (code, PPC_BR_ALWAYS, 0);
#if ENABLE_WRONG_METHOD_CHECK
					ppc_patch (item->jmp_code, code);
					ppc_break (code);
					item->jmp_code = NULL;
#endif
				}
			}
		} else {
			ppc_load (code, ppc_r0, (guint32)item->key);
			ppc_cmpl (code, 0, 0, MONO_ARCH_IMT_REG, ppc_r0);
			item->jmp_code = code;
			ppc_bc (code, PPC_BR_FALSE, PPC_BR_LT, 0);
		}
	}
	/* patch the branches to get to the target items */
	for (i = 0; i < count; ++i) {
		MonoIMTCheckItem *item = imt_entries [i];
		if (item->jmp_code) {
			if (item->check_target_idx) {
				ppc_patch (item->jmp_code, imt_entries [item->check_target_idx]->code_target);
			}
		}
	}

	if (!fail_tramp)
		mono_stats.imt_thunks_size += code - start;
	g_assert (code - start <= size);
	mono_arch_flush_icache (start, size);
	return start;
#endif
}

MonoMethod*
mono_arch_find_imt_method (gpointer *regs, guint8 *code)
{
	return (MonoMethod*) regs [MONO_ARCH_IMT_REG];
}

MonoObject*
mono_arch_find_this_argument (gpointer *regs, MonoMethod *method, MonoGenericSharingContext *gsctx)
{
	return mono_arch_get_this_arg_from_call (gsctx, mono_method_signature (method), (gssize*)regs, NULL);
}
#endif

MonoVTable*
mono_arch_find_static_call_vtable (gpointer *regs, guint8 *code)
{
	NOT_IMPLEMENTED;
	return (MonoVTable*) regs [MONO_ARCH_RGCTX_REG];
}
