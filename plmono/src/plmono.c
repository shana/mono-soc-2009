#include "postgres.h"
#include "executor/spi.h"
#include "commands/trigger.h"
#include "fmgr.h"
#include "access/heapam.h"
#include "utils/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum plmono_call_handler(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_call_handler);

extern Datum plmono_validator(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(plmono_validator);

Datum plmono_regular_handler(PG_FUNCTION_ARGS);
Form_pg_type getTypeFromOid(Oid type_oid);
Form_pg_proc getFunctionFromOid(Oid fn_oid);
MonoMethod* getMonoMethod(void);
void* convertDatumToMonoType(Datum val, Oid type_oid);
void releaseMonoValue(void *val);
Datum convertMonoTypeToDatum(void *mono_val, Oid type_oid);

static MonoDomain *domain = NULL;

Form_pg_type
getTypeFromOid(Oid type_oid)
{
	HeapTuple typeTup;
	Form_pg_type typeStruct;

	typeTup = SearchSysCache(TYPEOID, ObjectIdGetDatum(type_oid), 0, 0, 0);
	if (!HeapTupleIsValid(typeTup))
		elog(ERROR, "cache lookup failed for type %u", type_oid);

	typeStruct = (Form_pg_type) GETSTRUCT(typeTup);
	ReleaseSysCache(typeTup);
	
	return typeStruct;
}

Form_pg_proc
getFunctionFromOid(Oid fn_oid)
{
	HeapTuple procTup;
	Form_pg_proc procStruct;

	procTup = SearchSysCache(PROCOID, ObjectIdGetDatum(fn_oid), 0, 0, 0);
	if (!HeapTupleIsValid(procTup))
		elog(ERROR, "cache lookup failed for function %u", fn_oid);

	procStruct = (Form_pg_proc) GETSTRUCT(procTup);
  	ReleaseSysCache(procTup);

	return procStruct;
}

MonoMethod*
getMonoMethod(void)
{
	MonoAssembly *assembly;
	MonoImage *image;
	MonoClass *class;
	MonoMethod *method;
	MonoImageOpenStatus status;

	char *filename = "/home/kynlem/Projects/PLMono/debug/Test.dll";
	char *assembly_name = "PLMono";
	char *class_name = "Test";
	char *method_name = "Func";
	int method_nargs = 3;

	if (domain == NULL)
	{
		domain = mono_jit_init(filename);
	}

	assembly = mono_assembly_open(filename, &status);
	if (assembly == NULL)
	{
   		elog(ERROR, "Assembly %s not found", filename);
	}

	image = mono_assembly_get_image(assembly);

	class = mono_class_from_name(image, assembly_name, class_name);
	if (class == NULL)
	{
   		elog(ERROR, "Class %s not found", class_name);
	}

	method = mono_class_get_method_from_name(class, method_name, method_nargs);
	if (method == NULL)
	{
   		elog(ERROR, "Method %s taking %d arguments not found", method_name, method_nargs);
	}

	return method;
}

void*
convertDatumToMonoType(Datum val, Oid type_oid)
{
	void *mono_val = NULL;

	switch (type_oid)
	{
		case BOOLOID:
			mono_val = (int32*) palloc(sizeof(int32));
			*((int32*) mono_val) = DatumGetBool(val);
			break;

		case INT2OID:
			mono_val = (int16*) palloc(sizeof(int16));
			*((int16*) mono_val) = DatumGetInt16(val);
			break;

		case INT4OID:
			mono_val = (int32*) palloc(sizeof(int32));
			*((int32*) mono_val) = DatumGetInt32(val);
			break;

		case INT8OID:
			mono_val = (int64*) palloc(sizeof(int64));
			*((int64*) mono_val) = DatumGetInt64(val);
			break;

		case FLOAT4OID:
			mono_val = (float4*) palloc(sizeof(float4));
			*((float4*) mono_val) = DatumGetFloat4(val);
			break;

		case FLOAT8OID:
			mono_val = (float8*) palloc(sizeof(float8));
			*((float8*) mono_val) = DatumGetFloat8(val);
			break;
	}

	return mono_val;
}

void
releaseMonoValue(void *val)
{
	pfree(val);
}

Datum
convertMonoTypeToDatum(void *mono_val, Oid type_oid)
{
	Datum val = Int32GetDatum(0);

	switch (type_oid)
	{
		case BOOLOID:
			val = BoolGetDatum(*((int32*) mono_val));
			break;

		case INT2OID:
			val = Int16GetDatum(*((int16*) mono_val));
			break;

		case INT4OID:
			val = Int32GetDatum(*((int32*) mono_val));
			break;

		case INT8OID:
			val = Int64GetDatum(*((int64*) mono_val));
			break;

		case FLOAT4OID:
			val = Float4GetDatum(*((float4*) mono_val));
			break;

		case FLOAT8OID:
			val = Float8GetDatum(*((float8*) mono_val));
			break;
	}

	return val;
}

Datum
plmono_regular_handler(PG_FUNCTION_ARGS)
{
	MonoMethod *method;
	MonoObject *result;
	Form_pg_proc procStruct;
	gpointer args[FUNC_MAX_ARGS];
	int i;

	procStruct = getFunctionFromOid(fcinfo->flinfo->fn_oid);

	method = getMonoMethod();

	for (i = 0; i < fcinfo->nargs; i++)
	{
		args[i] = convertDatumToMonoType(fcinfo->arg[i], procStruct->proargtypes.values[i]);
	}

	result = mono_runtime_invoke(method, NULL, args, NULL);

	for (i = 0; i < fcinfo->nargs; i++)
	{
		releaseMonoValue(args[i]);
	}

	return convertMonoTypeToDatum(mono_object_unbox(result), procStruct->prorettype);
}

Datum
plmono_call_handler(PG_FUNCTION_ARGS)
{
	if (CALLED_AS_TRIGGER(fcinfo))
	{
  		elog(ERROR, "Feature not implemented");
	}

	return plmono_regular_handler(fcinfo);
}

Datum
plmono_validator(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(0);
}
