#include "ColumnStorage.h"
#include "SqliteColumnDataSource.h"
#include "integer64.h"


using namespace Rcpp;

ColumnStorage::ColumnStorage(DATA_TYPE dt_, const int capacity_, const int n_max_,
                             const SqliteColumnDataSource& source_)
  :
  i(0),
  dt(dt_),
  capacity(capacity_),
  n_max(n_max_),
  source(source_)
{
  data = allocate(capacity);
}

ColumnStorage::~ColumnStorage() {
}

ColumnStorage* ColumnStorage::append_col() {
  if (source.is_null()) return append_null();
  return append_data();
}

DATA_TYPE ColumnStorage::get_data_type() const {
  DATA_TYPE dt_final = dt;
  if (dt_final == DT_UNKNOWN) dt_final = source.get_decl_data_type();
  return dt_final;
}

SEXP ColumnStorage::allocate(const int length, DATA_TYPE dt) {
  switch (dt) {
  case DT_UNKNOWN:
    return R_NilValue;

  case DT_BOOL:
    return Rf_allocVector(LGLSXP, length);

  case DT_INT:
    return Rf_allocVector(INTSXP, length);

  case DT_INT64:
  {
    SEXP ret = Rf_allocVector(REALSXP, length);
    Rf_setAttrib(ret, R_ClassSymbol, Rf_ScalarString(Rf_mkChar("integer64")));
    return ret;
  }

  case DT_REAL:
    return Rf_allocVector(REALSXP, length);

  case DT_STRING:
    return Rf_allocVector(STRSXP, length);

  case DT_BLOB:
    return Rf_allocVector(VECSXP, length);

  default:
    stop("Unknown type %d", dt);
  }
}

int ColumnStorage::copy_to(SEXP x, DATA_TYPE dt, const int pos, const int n) const {
  int src, tgt;
  for (src = 0, tgt = pos; src < capacity && src < i && tgt < n; ++src, ++tgt) {
    if (Rf_isNull(data)) {
      fill_default_value(x, dt, tgt);
    }
    else {
      switch (dt) {
      case DT_INT:
        INTEGER(x)[tgt] = INTEGER(data)[src];
        break;

      case DT_INT64:
        switch (TYPEOF(data)) {
        case INTSXP:
          INTEGER64(x)[tgt] = INTEGER(data)[src];
          break;

        case REALSXP:
          INTEGER64(x)[tgt] = INTEGER64(data)[src];
          break;
        }
        break;

      case DT_REAL:
        REAL(x)[tgt] = REAL(data)[src];
        break;

      case DT_STRING:
        SET_STRING_ELT(x, tgt, STRING_ELT(data, src));
        break;

      case DT_BLOB:
        SET_VECTOR_ELT(x, tgt, VECTOR_ELT(data, src));
        break;

      default:
        stop("NYI: default");
      }
    }
  }

  for (; src < i && tgt < n; ++src, ++tgt) {
    fill_default_value(x, dt, tgt);
  }

  return src;
}

ColumnStorage* ColumnStorage::append_null() {
  if (i < capacity) fill_default_col_value();
  ++i;
  return this;
}

ColumnStorage* ColumnStorage::append_data() {
  if (dt == DT_UNKNOWN) return append_data_to_new(dt);
  if (i >= capacity) return append_data_to_new(dt);
  if (dt == DT_INT && source.get_data_type() == DT_INT64) return append_data_to_new(DT_INT64);

  fill_col_value();
  ++i;
  return this;
}

ColumnStorage* ColumnStorage::append_data_to_new(DATA_TYPE new_dt) {
  if (new_dt == DT_UNKNOWN) new_dt = source.get_data_type();

  int new_capacity;
  if (n_max < 0) new_capacity = capacity * 2;
  else new_capacity = std::max(n_max - i, 1);

  ColumnStorage* spillover = new ColumnStorage(new_dt, new_capacity, n_max, source);
  return spillover->append_data();
}

void ColumnStorage::fill_default_col_value() {
  fill_default_value(data, dt, i);
}

void ColumnStorage::fill_default_value(SEXP data, DATA_TYPE dt, R_xlen_t i) {
  switch (dt) {
  case DT_BOOL:
    LOGICAL(data)[i] = NA_LOGICAL;
    break;

  case DT_INT:
    INTEGER(data)[i] = NA_INTEGER;
    break;

  case DT_INT64:
    INTEGER64(data)[i] = NA_INTEGER64;
    break;

  case DT_REAL:
    REAL(data)[i] = NA_REAL;
    break;

  case DT_STRING:
    SET_STRING_ELT(data, i, NA_STRING);
    break;

  case DT_BLOB:
    SET_VECTOR_ELT(data, i, R_NilValue);
    break;
  }
}

void ColumnStorage::fill_col_value() {
  switch (dt) {
  case DT_INT:
    source.fetch_int(data, i);
    break;

  case DT_INT64:
    source.fetch_int64(data, i);
    break;

  case DT_REAL:
    source.fetch_real(data, i);
    break;

  case DT_STRING:
    source.fetch_string(data, i);
    break;

  case DT_BLOB:
    source.fetch_blob(data, i);
    break;

  default:
    stop("NYI");
  }
}

SEXP ColumnStorage::allocate(const int capacity) const {
  return allocate(capacity, dt);
}
