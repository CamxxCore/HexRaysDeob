#include <hexrays.hpp>
#include <kernwin.hpp>
#include <graph.hpp>
#include <lines.hpp>
#include <memory>
#include <vector>

#include "HexRaysUtil.hpp" // if you have helpers; otherwise remove

//------------------------------------------------------------------------------
// Minimal lifetime helper for mba
//------------------------------------------------------------------------------
struct mba_holder_t {
    mbl_array_t* mba = nullptr;
    mba_holder_t() = default;
    explicit mba_holder_t( mbl_array_t* p ) : mba( p ) {}
    ~mba_holder_t() { if ( mba != nullptr ) delete mba; }
    mba_holder_t( const mba_holder_t& ) = delete;
    mba_holder_t& operator=( const mba_holder_t& ) = delete;
    mba_holder_t( mba_holder_t&& o ) noexcept : mba( o.mba ) { o.mba = nullptr; }
    mba_holder_t& operator=( mba_holder_t&& o ) noexcept
    {
        if ( this != &o )
        {
            if ( mba != nullptr ) delete mba;
            mba = o.mba;
            o.mba = nullptr;
        }
        return *this;
    }
};

//------------------------------------------------------------------------------
// Hex-Rays printer -> strvec_t dumper (strips color escapes, optional indent)
//------------------------------------------------------------------------------
 // Fix the return type of the overridden `print` method in `mblock_virtual_dumper_t`  
    // to match the return type of the base class `vd_printer_t::print`.  
    // Change `ssize_t` to `int` to resolve both E0317 and C2555 errors.  

struct mblock_virtual_dumper_t : public vd_printer_t {
    strvec_t* lines;
    int indent = 0;

    explicit mblock_virtual_dumper_t( strvec_t* out, int idnt = 0 )
        : lines( out ), indent( idnt ) {
    }

    AS_PRINTF( 3, 4 ) int idaapi print( int /*level*/,
        const char* format,
        ... ) override {
        qstring buf;
        {
            va_list va;
            va_start( va, format );
            buf.vsprnt( format, va );
            va_end( va );
        }

        // Strip inline color escapes (still used by SDK).  
        static const char pfx_on[] = { COLOR_ON, COLOR_PREFIX, 0 };
        static const char pfx_off[] = { COLOR_OFF, COLOR_PREFIX, 0 };
        buf.replace( pfx_on, "" );
        buf.replace( pfx_off, "" );

        // Left pad.  
        if ( indent > 0 ) {
            qstring pad;
            pad.fill( ' ', indent );
            buf.insert( 0, pad.c_str() );
        }

        lines->push_back( simpleline_t( buf ) );
        return static_cast<int>( buf.length() );
    }
};

//------------------------------------------------------------------------------
// Per-view state
//------------------------------------------------------------------------------
struct micro_view_t {
    mba_holder_t   mba;
    strvec_t       lines;
    TWidget* cv = nullptr;
    TWidget* blk_gv = nullptr; // block graph viewer
    qstring        title;

    explicit micro_view_t( mbl_array_t* m, qstring t )
        : mba( m ), title( std::move( t ) ) {
    }
};

//------------------------------------------------------------------------------
// Block graph text helpers
//------------------------------------------------------------------------------
static qstring block_name( int b )
{
    qstring s;
    s.sprnt( "B%d", b );
    return s;
}

//------------------------------------------------------------------------------
// Graph callbacks (block graph)
//------------------------------------------------------------------------------

static ssize_t idaapi mgr_callback( void* ud, int code, va_list va)
{
    micro_view_t* mv = static_cast<micro_view_t*>( ud );
    if ( mv == nullptr || mv->mba.mba == nullptr )
        return 0;

    switch ( code )
    {
    case grcode_user_refresh:
    {
        // args: mutable_graph_t *g
        auto* g = va_arg( va, interactive_graph_t* );
        if ( g == nullptr )
            return 0;

        g->clear();
        // Create one node per micro-block; add edges following succset.
        const int n = mv->mba.mba->qty;
        g->resize( n );
        for ( int i = 0; i < n; ++i )
        {
            mblock_t* b = mv->mba.mba->get_mblock( i );
            if ( b == nullptr ) continue;
            for ( auto j = b->succset.begin(); j != b->succset.end(); j++ )
                g->add_edge( i, *j, nullptr );
        }
        return 1;
    }

    case grcode_user_text:
    {
        // args: mutable_graph_t *g, int node, const char **result
        (void)va_arg( va, interactive_graph_t* );
        const int node = va_arg( va, int );
        const char** out = va_arg( va, const char** );
        static qstring tmp;
        tmp = block_name( node );
        *out = tmp.c_str();
        return 1;
    }

    default:
        break;
    }
    return 0;
}

//------------------------------------------------------------------------------
// UI hook: close graphs when text view goes away
//------------------------------------------------------------------------------
static ssize_t idaapi ui_callback( void* ud, int notification_code, va_list va )
{
    micro_view_t* mv = static_cast<micro_view_t*>( ud );
    if ( mv == nullptr )
        return 0;

    switch ( notification_code )
    {
    case ui_widget_invisible:
    {
        TWidget* w = va_arg( va, TWidget* );
        if ( w == mv->cv )
        {
            if ( mv->blk_gv != nullptr )
                close_widget( mv->blk_gv, WCLS_SAVE | WCLS_CLOSE_LATER );
            unhook_from_notification_point( HT_UI, ui_callback, mv );
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Keyboard for custom text view
//   G: show/rebuild block graph
//------------------------------------------------------------------------------
static bool idaapi cv_keyboard( TWidget* v, int key, int /*shift*/, void* ud )
{
    micro_view_t* mv = static_cast<micro_view_t*>( ud );
    if ( mv == nullptr || v != mv->cv )
        return false;

    if ( key == 'g' || key == 'G' )
    {
        if ( mv->blk_gv == nullptr )
        {
            // parent container for GV (optional)
            TWidget* empty = create_empty_widget( block_name( -1 ).c_str() );
            if ( empty == nullptr )
                return false;

            const int title_h = 12;
            mv->blk_gv = create_graph_viewer( "Microcode Blocks", 0, mgr_callback, mv, title_h, empty );
            if ( mv->blk_gv == nullptr )
            {
                close_widget( empty, WCLS_SAVE | WCLS_CLOSE_LATER );
                return false;
            }

            // Layout & show
            display_widget( empty, WOPN_DP_TAB | WOPN_RESTORE | WOPN_CLOSED_BY_ESC );
            display_widget( mv->blk_gv, WOPN_DP_TAB | WOPN_RESTORE | WOPN_CLOSED_BY_ESC );
            viewer_fit_window( mv->blk_gv );
        }
        else
        {
            // Force refresh of existing GV
           // TWidget* gv_parent = get_widget_parent( mv->blk_gv );
           // if ( gv_parent != nullptr )
             //   display_widget( gv_parent, WOPN_RESTORE | WOPN_CLOSED_BY_ESC );
            display_widget( mv->blk_gv, WOPN_RESTORE | WOPN_CLOSED_BY_ESC );
            viewer_fit_window( mv->blk_gv );
            refresh_viewer( mv->blk_gv );
        }
        return true;
    }

    return false;
}

static bool idaapi cv_popup( TWidget*, void*, void* )
{
    // No popup customization
    return false;
}

static bool idaapi cv_double( TWidget*, int, void* )
{
    return false;
}

static bool idaapi cv_mouse( TWidget*, int, int, int, void* )
{
    return false;
}

//------------------------------------------------------------------------------
// Dump microcode to strvec
//------------------------------------------------------------------------------
static void dump_microcode_lines( mbl_array_t* mba, strvec_t& out, int indent = 0 )
{
    mblock_virtual_dumper_t pd( &out, indent );
    // Print full MBA; Hex-Rays will call vd_printer_t::print for each line.
    // Flags default to verbose text.
    mba->print( pd );
}

//------------------------------------------------------------------------------
// Entrypoint: build text view, hook UI, add keyboard
//------------------------------------------------------------------------------
void ShowMicrocodeExplorer( mba_maturity_t mmat = MMAT_LVARS )
{
    if ( !init_hexrays_plugin() )
    {
        warning( "Hex-Rays decompiler is not available." );
        return;
    }

    const ea_t ea = get_screen_ea();
    func_t* pfn = get_func( ea );
    if ( pfn == nullptr )
    {
        warning( "No function at the cursor." );
        return;
    }

    hexrays_failure_t hf; // IDA 9 failure info
    mbl_array_t* mba = gen_microcode( pfn, &hf, 0, mmat );
    if ( mba == nullptr )
    {
        warning( "gen_microcode() failed: %s", hf.str.c_str() );
        return;
    }

    qstring ttl;
    ttl.sprnt( "Microcode @ %a (mmat=%d)", pfn->start_ea, int( mmat ) );

    auto* mv = new micro_view_t( mba, ttl );
    dump_microcode_lines( mv->mba.mba, mv->lines, 0 );

    simpleline_place_t pl_begin;
    simpleline_place_t pl_end( mv->lines.size() > 0 ? mv->lines.size() - 1 : 0 );

    static const custom_viewer_handlers_t handlers(
        nullptr,           // keyboard? we supply below via set_custom_viewer_handlers
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr );

    // Create code viewer bound to our strvec
    mv->cv = create_custom_viewer( mv->title.c_str(),
        &pl_begin, &pl_end, &pl_begin,
        nullptr,
        &mv->lines,
        nullptr,
        nullptr );
    if ( mv->cv == nullptr )
    {
        delete mv;
        return;
    }

    // Install specific handlers
    set_custom_viewer_handlers( mv->cv, &handlers, mv );
    set_code_viewer_handler( mv->cv, CVH_KEYDOWN, (void*)cv_keyboard );
    set_code_viewer_handler( mv->cv, CVH_POPUP, (void*)cv_popup );
    set_code_viewer_handler( mv->cv, CVH_DBLCLICK, (void*)cv_double );
    set_code_viewer_handler( mv->cv, CVH_MOUSEMOVE, (void*)cv_mouse );

    // Hook UI so we can close graph(s) when text view goes away
    hook_to_notification_point( HT_UI, ui_callback, mv );

    display_widget( mv->cv, WOPN_RESTORE | WOPN_CLOSED_BY_ESC | WOPN_DP_TAB );
}

//------------------------------------------------------------------------------
// Optional convenience shim (kept for compatibility with your existing code)
//------------------------------------------------------------------------------
void ShowMicrocodeExplorer()
{
    ShowMicrocodeExplorer( MMAT_LVARS );
}
