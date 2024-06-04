#include <win/win16.h>


HINSTANCE MyInstance;
HWND MainDialogWnd = 0;


BOOL __export FAR PASCAL MainWindowDlg( HWND win_handle, unsigned msg,
                               UINT wparam, LONG lparam )
{
    lparam = lparam;                    /* turn off warning */

    switch( msg ) {
    case WM_INITDIALOG:
        MainDialogWnd = win_handle;
        return( TRUE );

    case WM_COMMAND:
        if( LOWORD(wparam) == IDCANCEL ) {
            EndDialog( win_handle, TRUE );
            return( TRUE );
        }
        break;
    }
    return( FALSE );

}



static void DisplayDialog( char *name, BOOL __export FAR PASCAL rtn(HWND, unsigned, UINT, LONG) )
{
    FARPROC     proc;

    proc = MakeProcInstance( (FARPROC)rtn, MyInstance );
    DialogBox( MyInstance, name, MainDialogWnd, (DLGPROC)proc );
    FreeProcInstance( proc );
}


int PASCAL WinMain( HINSTANCE this_inst, HINSTANCE prev_inst, LPSTR cmdline,
                    int cmdshow )
{
    (void)cmdline;
    (void)cmdshow;

    MyInstance = this_inst;
    if (prev_inst) {
        return 0;
    }

    DisplayDialog("DLG_TOOLBOX", MainWindowDlg);

    return 0;
}
