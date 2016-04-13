
#include "elastos/droid/database/sqlite/CSQLiteQuery.h"
#include <elastos/utility/logging/Slogger.h>

using Elastos::Utility::Logging::Slogger;

namespace Elastos {
namespace Droid {
namespace Database {
namespace Sqlite {

const String CSQLiteQuery::TAG("SQLiteQuery");

CAR_INTERFACE_IMPL(CSQLiteQuery, SQLiteProgram, ISQLiteQuery);

CAR_OBJECT_IMPL(CSQLiteQuery)

ECode CSQLiteQuery::FillWindow(
    /* [in] */ ICursorWindow* window,
    /* [in] */ Int32 startPos,
    /* [in] */ Int32 requiredPos,
    /* [in] */ Boolean countAllRows,
    /* [out] */ Int32* result)
{
    VALIDATE_NOT_NULL(result)

    AcquireReference();
    //try {
    ECode ec = ISQLiteClosable::Probe(window)->AcquireReference();
    if (FAILED(ec)) {
        ReleaseReference();
        *result = -1;
        return ec;
    }
    //try {
    AutoPtr<SQLiteSession> session;
    GetSession((SQLiteSession**)&session);
    Int32 numRows;
    ec = session->ExecuteForCursorWindow(GetSql(), GetBindArgs(), window, startPos, requiredPos, countAllRows,
            GetConnectionFlags(), mCancellationSignal, &numRows);
    if (ec == (ECode)E_SQLITE_DATABASE_CORRUPT_EXCEPTION) {
        OnCorruption();
    }
    else if (ec == (ECode)E_SQLITE_EXCEPTION) {
        Slogger::E(TAG, "exception: 0x%08x; query: %s", ec, GetSql().string());
    }
    // } catch (SQLiteDatabaseCorruptException ex) {
    //     onCorruption();
    //     throw ex;
    // } catch (SQLiteException ex) {
    //     Log.e(TAG, "exception: " + ex.getMessage() + "; query: " + getSql());
    //     throw ex;
    // } finally {
    //     window.releaseReference();
    // }
    //} finally {
    //    releaseReference();
    //}
    *result = numRows;
    ISQLiteClosable::Probe(window)->ReleaseReference();
    ReleaseReference();
    return ec;
}

ECode CSQLiteQuery::ToString(
    /* [out] */ String* value)
{
    VALIDATE_NOT_NULL(value)
    *value = String("SQLiteQuery") + GetSql();
    return NOERROR;
}

ECode CSQLiteQuery::constructor(
    /* [in] */ ISQLiteDatabase* db,
    /* [in] */ const String& query,
    /* [in] */ ICancellationSignal* cancellationSignal)
{
    mCancellationSignal = cancellationSignal;
    return SQLiteProgram::constructor(db, query, NULL, cancellationSignal);
}

} //Sqlite
} //Database
} //Droid
} //Elastos