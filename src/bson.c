#include <mongolite.h>

SEXP ConvertArray(bson_iter_t* iter, bson_iter_t* counter);
SEXP ConvertObject(bson_iter_t* iter, bson_iter_t* counter);
SEXP ConvertValue(bson_iter_t* iter);
SEXP ConvertBinary(bson_iter_t* iter);
SEXP ConvertDate(bson_iter_t* iter);
SEXP ConvertDec128(bson_iter_t* iter);
SEXP ConvertTimestamp(bson_iter_t* iter);

SEXP R_json_to_bson(SEXP json){
  bson_t *b;
  bson_error_t err;

  b = bson_new_from_json ((uint8_t*)translateCharUTF8(asChar(json)), -1, &err);
  if(!b)
    stop(err.message);

  return bson2r(b);
}

SEXP R_raw_to_bson(SEXP buf){
  bson_error_t err;
  bson_t *b = bson_new_from_data(RAW(buf), length(buf));
  if(!b)
    stop(err.message);
  return bson2r(b);
}

SEXP R_bson_to_json(SEXP ptr){
  return mkStringUTF8(bson_as_json (r2bson(ptr), NULL));
}

SEXP R_bson_to_raw(SEXP ptr){
  bson_t *b = r2bson(ptr);
  const uint8_t *buf = bson_get_data(b);
  return mkRaw(buf, b->len);
}

SEXP R_bson_to_list(SEXP ptr) {
  bson_t *b = r2bson(ptr);
  return bson2list(b);
}

SEXP ConvertValue(bson_iter_t* iter){
  if(BSON_ITER_HOLDS_INT32(iter)){
    int res = bson_iter_int32(iter);
    return res == NA_INTEGER ? ScalarReal(res) : ScalarInteger(res);
  } else if(BSON_ITER_HOLDS_NULL(iter)){
    return R_NilValue;
  } else if(BSON_ITER_HOLDS_BOOL(iter)){
    return ScalarLogical(bson_iter_bool(iter));
  } else if(BSON_ITER_HOLDS_DOUBLE(iter)){
    return ScalarReal(bson_iter_double(iter));
  } else if(BSON_ITER_HOLDS_INT64(iter)){
    return ScalarReal((double) bson_iter_int64(iter));
  } else if(BSON_ITER_HOLDS_UTF8(iter)){
    return mkStringUTF8(bson_iter_utf8(iter, NULL));
  } else if(BSON_ITER_HOLDS_CODE(iter)){
    return mkStringUTF8(bson_iter_code(iter, NULL));
  } else if(BSON_ITER_HOLDS_BINARY(iter)){
    return ConvertBinary(iter);
  } else if(BSON_ITER_HOLDS_DATE_TIME(iter)){
    return ConvertDate(iter);
  } else if(BSON_ITER_HOLDS_DECIMAL128(iter)){
    return ConvertDec128(iter);
  } else if(BSON_ITER_HOLDS_TIMESTAMP(iter)){
    return ConvertTimestamp(iter);
  } else if(BSON_ITER_HOLDS_OID(iter)){
    const bson_oid_t *val = bson_iter_oid(iter);
    char str[25];
    bson_oid_to_string(val, str);
    return mkString(str);
  } else if(BSON_ITER_HOLDS_ARRAY(iter)){
    bson_iter_t child1;
    bson_iter_t child2;
    bson_iter_recurse (iter, &child1);
    bson_iter_recurse (iter, &child2);
    return ConvertArray(&child1, &child2);
  } else if(BSON_ITER_HOLDS_DOCUMENT(iter)){
    bson_iter_t child1;
    bson_iter_t child2;
    bson_iter_recurse (iter, &child1);
    bson_iter_recurse (iter, &child2);
    return ConvertObject(&child1, &child2);
  } else {
    stop("Unimplemented BSON type %d\n", bson_iter_type(iter));
  }
}

SEXP ConvertTimestamp(bson_iter_t* iter){
  uint32_t timestamp;
  uint32_t increment;
  bson_iter_timestamp(iter, &timestamp, &increment);
  SEXP res = PROTECT(allocVector(VECSXP, 2));
  SET_VECTOR_ELT(res, 0, ScalarInteger(timestamp));
  SET_VECTOR_ELT(res, 1, ScalarInteger(increment));
  SEXP names = PROTECT(allocVector(STRSXP, 2));
  SET_STRING_ELT(names, 0, Rf_mkChar("t"));
  SET_STRING_ELT(names, 1, Rf_mkChar("i"));
  setAttrib(res, R_NamesSymbol, names);
  UNPROTECT(2);
  return res;
}

SEXP ConvertDate(bson_iter_t* iter){
  SEXP list = PROTECT(allocVector(VECSXP, 1));
  SET_VECTOR_ELT(list, 0, ScalarReal((double) bson_iter_date_time(iter)));
  setAttrib(list, R_NamesSymbol, mkString("$date"));
  UNPROTECT(1);
  return list;
}

SEXP ConvertDec128(bson_iter_t* iter){
  bson_decimal128_t decimal128;
  bson_iter_decimal128(iter, &decimal128);
  char string[BSON_DECIMAL128_STRING];
  bson_decimal128_to_string (&decimal128, string);
  return mkString(string);
}

SEXP ConvertBinary(bson_iter_t* iter){
  bson_subtype_t subtype;
  uint32_t binary_len;
  const uint8_t *binary;
  bson_iter_binary(iter, &subtype, &binary_len, &binary);

  //create raw vector
  SEXP out = PROTECT(allocVector(RAWSXP, binary_len));
  for (int i = 0; i < binary_len; i++) {
    RAW(out)[i] = binary[i];
  }
  setAttrib(out, install("type"), ScalarRaw(subtype));
  UNPROTECT(1);
  return out;

}

SEXP ConvertArray(bson_iter_t* iter, bson_iter_t* counter){
  SEXP ret;
  int count = 0;
  while(bson_iter_next(counter)){
    count++;
  }
  PROTECT(ret = allocVector(VECSXP, count));
  for (int i = 0; bson_iter_next(iter); i++) {
    SET_VECTOR_ELT(ret, i, ConvertValue(iter));
  }
  UNPROTECT(1);
  return ret;
}

SEXP ConvertObject(bson_iter_t* iter, bson_iter_t* counter){
  SEXP names;
  SEXP ret;
  int count = 0;
  while(bson_iter_next(counter)){
    count++;
  }
  PROTECT(ret = allocVector(VECSXP, count));
  PROTECT(names = allocVector(STRSXP, count));
  for (int i = 0; bson_iter_next(iter); i++) {
    SET_STRING_ELT(names, i, mkChar(bson_iter_key(iter)));
    SET_VECTOR_ELT(ret, i, ConvertValue(iter));
  }
  setAttrib(ret, R_NamesSymbol, names);
  UNPROTECT(2);
  return ret;
}
