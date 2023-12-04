/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,WITHOUT WARRANTY OF ANY KIND,either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * Alternatively,the contents of this file may be used under the terms
 * of the GNU General Public License(the "GPL"),in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License,indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above,a recipient may use your
 * version of this file under either the License or the GPL.
 *
 */

/*
 * nschartdir.c -- Interface to ChartDirector C++ library
 *
 * How it works
 *
 *  ns_chartdir command is used to open create charts
 *
 *  ns_chartdir usage:
 *
 *    ns_chartdir charts
 *      returns list with cusrrently opened charts as
 *         { id accesstime } ...
 *
 *    ns_chartdir gc
 *      performs garbage collection, closes inactive charts according to
 *      config parameter timeout from config section ns/server/${server}/module/nschartdir
 *
 *
 * Authors
 *
 *     Vlad Seryakov vlad@crystalballinc.com
 */

#define USE_TCL8X

extern "C" {
#include "ns.h"
}

#include "chartdir.h"

#define _VERSION           "0.9.6"

#define MAX_LAYERS         5

enum ChartType { XYChartType, PieChartType };
enum LayerType { LineType, BarType, AreaType, TrendType, PieType };

typedef struct _Chart {
    struct _Chart *next, *prev;
    unsigned long id;
    time_t access_time;
    ChartType type;
    BaseChart *chart;
    XYChart *xy;
    PieChart *pie;
    PlotArea *plotarea;
    struct {
        LayerType type;
        Layer *layer;
        BarLayer *bar;
        LineLayer *line;
        TrendLayer *trend;
    } layers[MAX_LAYERS];
} Ns_Chart;

static int ChartCmd(ClientData arg, Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[]);
static int ChartInterpInit(Tcl_Interp * interp, const void *context);
static void ChartGC(void *arg);

static Ns_Chart *chartList = 0;
static Ns_Mutex chartMutex;
static int chartIdleTimeout = 600;
static int chartGCInterval = 600;
static unsigned long chartID = 0;

static const char *chartAligments[] = { "Bottom", "2",
    "BottomLeft", "1",
    "BottomCenter", "2",
    "BottomRight", "3",
    "Left", "4",
    "Center", "5",
    "Right", "6",
    "Top", "8",
    "TopLeft", "7",
    "TopCenter", "8",
    "TopRight", "9",
    0
};

static const char *chartLineTypes[] = { "DashLine", "1285",     // 0x505
    "DotLine", "514",           // 0x202
    "DotDashLine", "84214277",  // 0x05050205
    "AltDashLine", "168101125", // 0x0A050505
    0
};

static const char *chartSymbolTypes[] = { "NoSymbol",
    "SquareSymbol",
    "DiamondSymbol",
    "TriangleSymbol",
    "RightTriangleSymbol",
    "LeftTriangleSymbol",
    "InvertedTriangleSymbol",
    "CircleSymbol",
    "CrossSymbol",
    "Cross2Symbol",
    0
};

static const char *chartDataCombineMethods[] = { "Overlay", "Stack", "Depth", "Side", 0 };

static const char *chartLegendModes[] = { "NormalLegend", "ReverseLegend", "NoLegend", 0 };

extern "C" {

    NS_EXPORT int Ns_ModuleVersion = 1;

/*
 * Load the config parameters, setup the structures
 */

    NS_EXPORT int Ns_ModuleInit(const char *server, const char *module) {
        const char *path;

         Ns_Log(Notice, "nschartdir module version %s server: %s", _VERSION, server);

         path = Ns_ConfigGetPath(server, module, NULL);
         Ns_ConfigGetInt(path, "idle_timeout", &chartIdleTimeout);
         Ns_ConfigGetInt(path, "gc_interval", &chartGCInterval);
        /* Schedule garbage collection proc for automatic chart close/cleanup */
        if (chartGCInterval > 0) {
            Ns_Time interval;
            interval.sec = chartGCInterval;  interval.usec = 0;
            Ns_ScheduleProcEx((Ns_SchedProc *)ChartGC, 0, NS_SCHED_THREAD, &interval, NULL);
            Ns_Log(Notice, "ns_chartdir: scheduling GC proc for every %d secs", chartGCInterval);
        }
        Ns_MutexSetName2(&chartMutex, "nschartdir", "chart");
        Ns_TclRegisterTrace(server, ChartInterpInit, 0, NS_TCL_TRACE_CREATE);
        return NS_OK;
    }

}

/*
 * Add ns_chartdir commands to interp.
 */
static int ChartInterpInit(Tcl_Interp * interp, const void *context)
{
    Tcl_CreateObjCommand(interp, "ns_chartdir", ChartCmd, NULL, NULL);
    return NS_OK;
}

static Ns_Chart *getChart(unsigned long id)
{
    Ns_Chart *chart;

    Ns_MutexLock(&chartMutex);
    for (chart = chartList; chart; chart = chart->next)
        if (chart->id == id)
            break;
    Ns_MutexUnlock(&chartMutex);
    return chart;
}

static void freeChart(Ns_Chart * chart, int lock)
{
    if (!chart)
        return;

    if (lock)
        Ns_MutexLock(&chartMutex);
    if (chart->prev)
        chart->prev->next = chart->next;
    if (chart->next)
        chart->next->prev = chart->prev;
    if (chart == chartList)
        chartList = chart->next;
    if (lock)
        Ns_MutexUnlock(&chartMutex);
    chart->chart->destroy();
    ns_free(chart);
}

// Garbage collection routine, closes expired charts
static void ChartGC(void *arg)
{
    Ns_Chart *chart;
    time_t now = time(0);

    Ns_MutexLock(&chartMutex);
    for (chart = chartList; chart;) {
        if (now - chart->access_time > chartIdleTimeout) {
            Ns_Chart *next = chart->next;
            Ns_Log(Notice, "ns_chartdir: GC: inactive chart %ld", chart->id);
            freeChart(chart, 0);
            chart = next;
            continue;
        }
        chart = chart->next;
    }
    Ns_MutexUnlock(&chartMutex);
}

static Alignment chartAlignment(const char *name, Alignment defalign = Center)
{
    if (!name)
        return defalign;
    for (int i = 0; chartAligments[i]; i += 2)
        if (!strcasecmp(chartAligments[i], name))
            return (Alignment) atoi(chartAligments[i + 1]);
    return defalign;
}

static int chartColor(Tcl_Interp * interp, Tcl_Obj * obj, int *color)
{
    char *name = Tcl_GetStringFromObj(obj, 0);

    if (!strcasecmp(name, "Transparent"))
        *color = Transparent;
    else if (!strcasecmp(name, "Palette"))
        *color = Palette;
    else if (!strcasecmp(name, "BackgroundColor"))
        *color = BackgroundColor;
    else if (!strcasecmp(name, "TextColor"))
        *color = TextColor;
    else if (!strcasecmp(name, "LineColor"))
        *color = LineColor;
    else if (!strcasecmp(name, "DataColor"))
        *color = DataColor;
    else if (!strcasecmp(name, "SameAsMainColor"))
        *color = SameAsMainColor;
    else
        return Tcl_GetIntFromObj(interp, obj, color);
    return TCL_OK;
}

static Ns_Chart *createChart(int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    Ns_Chart *chart;
    char *type;
    int width = 500;
    int height = 300;
    int bgcolor = 0xFFFFFF;
    int edgecolor = -1;
    int border = 0;

    if (objc < 5 ||
        !(type = Tcl_GetStringFromObj(objv[2], 0)) ||
        Tcl_GetIntFromObj(interp, objv[3], &width) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[4], &height) != TCL_OK ||
        (objc > 5 && chartColor(interp, objv[5], &bgcolor) != TCL_OK) ||
        (objc > 6 && chartColor(interp, objv[6], &edgecolor) != TCL_OK) ||
        (objc > 7 && Tcl_GetIntFromObj(interp, objv[7], &border) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv, "type width height ?bgcolor? ?edgecolor? ?border?");
        return 0;
    }
    chart = (Ns_Chart *) ns_calloc(1, sizeof(Ns_Chart));
    Ns_MutexLock(&chartMutex);
    chart->id = ++chartID;
    Ns_MutexUnlock(&chartMutex);

    if (!strcasecmp(type, "pie")) {
        chart->pie = PieChart::create(width, height);
        chart->chart = chart->pie;
        chart->type = PieChartType;
    } else {
        chart->xy = XYChart::create(width, height);
        chart->chart = chart->xy;
        chart->type = XYChartType;
    }
    chart->chart->setBackground(bgcolor, edgecolor, border);

    /* Link new chart to global chart list */
    Ns_MutexLock(&chartMutex);
    chart->next = chartList;
    if (chartList)
        chartList->prev = chart;
    chartList = chart;
    chart->access_time = time(0);
    Ns_MutexUnlock(&chartMutex);
    /* Return chart id */
    Tcl_SetObjResult(interp, Tcl_NewIntObj(chart->id));
    return chart;
}

static int setBackground(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int bgcolor;
    int edgecolor = -1;
    int border = 0;

    if (objc < 4 ||
        chartColor(interp, objv[3], &bgcolor) != TCL_OK ||
        (objc > 4 && chartColor(interp, objv[4], &edgecolor) != TCL_OK) ||
        (objc > 5 && Tcl_GetIntFromObj(interp, objv[5], &border) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart bgcolor ?edgecolor? ?border?");
        return TCL_ERROR;
    }
    chart->chart->setBackground(bgcolor, edgecolor, border);
    return TCL_OK;
}

static int setSize(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int width, height;

    if (objc < 5 ||
        Tcl_GetIntFromObj(interp, objv[3], &width) != TCL_OK || Tcl_GetIntFromObj(interp, objv[4], &height) != TCL_OK) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart width height");
        return TCL_ERROR;
    }
    chart->chart->setSize(width, height);
    return TCL_OK;
}

static int setPlotArea(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int x, y, width, height;
    int bgcolor = Transparent;
    int abgcolor = -1;
    int edgecolor = LineColor;
    int hgridcolor = 0xc0c0c0;
    int vgridcolor = Transparent;

    if (objc < 6 ||
        Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[5], &width) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[6], &height) != TCL_OK ||
        (objc > 7 && chartColor(interp, objv[7], &bgcolor) != TCL_OK) ||
        (objc > 8 && chartColor(interp, objv[8], &abgcolor) != TCL_OK) ||
        (objc > 9 && chartColor(interp, objv[9], &edgecolor) != TCL_OK) ||
        (objc > 10 && chartColor(interp, objv[10], &hgridcolor) != TCL_OK) ||
        (objc > 11 && chartColor(interp, objv[11], &vgridcolor) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv,
                         "#chart x y width height ?bgcolor? ?altbgcolor? ?edgecolor? ?hgridcolor? ?vgridcolor?");
        return TCL_ERROR;
    }
    chart->plotarea = chart->xy->setPlotArea(x, y, width, height, bgcolor, abgcolor, edgecolor, hgridcolor, vgridcolor);
    return TCL_OK;
}

static int addLegend(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int x, y;
    int vertical = true;
    int bgcolor = Transparent;
    int edgecolor = -1;
    int fontcolor = TextColor;
    int fontangle = 0;
    double fontheight = 8;
    const char *font = 0;
    const char *align = 0;

    if (objc < 5 ||
        Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK ||
        (objc > 5 && Tcl_GetIntFromObj(interp, objv[5], &vertical) != TCL_OK) ||
        (objc > 6 && chartColor(interp, objv[6], &bgcolor) != TCL_OK) ||
        (objc > 7 && chartColor(interp, objv[7], &edgecolor) != TCL_OK) ||
        (objc > 8 && !(font = Tcl_GetStringFromObj(objv[8], 0))) ||
        (objc > 9 && Tcl_GetDoubleFromObj(interp, objv[9], &fontheight) != TCL_OK) ||
        (objc > 10 && chartColor(interp, objv[10], &fontcolor) != TCL_OK) ||
        (objc > 11 && Tcl_GetIntFromObj(interp, objv[11], &fontangle) != TCL_OK) ||
        (objc > 12 && !(align = Tcl_GetStringFromObj(objv[12], 0)))) {
        Tcl_WrongNumArgs(interp, 2, objv,
                         "#chart x y ?vertical? ?bgcolor? ?edgecolor? ?font? ?fontheight? ?fontcolor? ?fontangle? ?align?");
        return TCL_ERROR;
    }
    chart->chart->addLegend(x, y, (bool) vertical, font, fontheight);
    chart->chart->getLegend()->setBackground(bgcolor, edgecolor);
    chart->chart->getLegend()->setFontColor(fontcolor);
    if (align)
        chart->chart->getLegend()->setAlignment(chartAlignment(align, TopLeft));
    if (fontangle)
        chart->chart->getLegend()->setFontAngle(fontangle);
    return TCL_OK;
}

static int addTitle(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    const char *title;
    const char *font = 0;
    const char *align = "Top";
    int fontheight = 12;
    int fontcolor = TextColor;
    int bgcolor = Transparent;
    int edgecolor = Transparent;
    int border = 0;

    if (objc < 4 ||
        !(title = Tcl_GetStringFromObj(objv[3], 0)) ||
        (objc > 4 && !(align = Tcl_GetStringFromObj(objv[4], 0))) ||
        (objc > 5 && !(font = Tcl_GetStringFromObj(objv[5], 0))) ||
        (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &fontheight) != TCL_OK) ||
        (objc > 7 && chartColor(interp, objv[7], &fontcolor) != TCL_OK) ||
        (objc > 8 && chartColor(interp, objv[8], &bgcolor) != TCL_OK) ||
        (objc > 9 && chartColor(interp, objv[9], &edgecolor) != TCL_OK) ||
        (objc > 10 && Tcl_GetIntFromObj(interp, objv[10], &border) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv,
                         "#chart title ?alignment? ?font? ?fontheight? ?fontcolor? ?bgcolor? ?edgecolor? ?border?");
        return TCL_ERROR;
    }
    chart->chart->addTitle(chartAlignment(align), title, font, fontheight, fontcolor)->setBackground(bgcolor, edgecolor,
                                                                                                     border);
    return TCL_OK;
}

static int setBgImage(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    const char *image;
    Alignment align = Center;

    if (objc < 4 || !(image = Tcl_GetStringFromObj(objv[3], 0))) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart name ?align? ?-plotarea?");
        return TCL_ERROR;
    }
    if (objc > 4)
        align = chartAlignment(Tcl_GetStringFromObj(objv[4], 0));
    if (objc < 6)
        chart->chart->setBgImage(image, align);
    else if (chart->plotarea)
        chart->plotarea->setBackground(image, align);
    return TCL_OK;
}

static int setColors(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    char *name;
    int *palette = 0;
    int argc = 0;
    Tcl_Obj **argv;

    if (objc < 4 || !(name = Tcl_GetStringFromObj(objv[3], 0))) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart palette");
        return TCL_ERROR;
    }

    if (!strcasecmp(name, "defaultPalette"))
        palette = (int *) defaultPalette;
    else if (!strcasecmp(name, "whiteOnBlackPalette"))
        palette = (int *) whiteOnBlackPalette;
    else if (!strcasecmp(name, "transparentPalette"))
        palette = (int *) transparentPalette;
    else {
        if (Tcl_ListObjGetElements(interp, objv[3], &argc, &argv) != TCL_OK) {
            Tcl_WrongNumArgs(interp, 2, objv, "palette");
            return TCL_ERROR;
        }
        if (argc > 0) {
            palette = (int *) ns_malloc(argc * sizeof(int));
            for (int i = 0; i < argc; i++)
                Tcl_GetIntFromObj(interp, argv[i], &palette[i]);
        }
    }
    chart->chart->setColors(palette);
    if (argc > 0)
        ns_free(palette);
    return TCL_OK;
}

static int setWallpaper(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    const char *image;
    if (objc < 4 || !(image = Tcl_GetStringFromObj(objv[3], 0))) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart name");
        return TCL_ERROR;
    }
    chart->chart->setWallpaper(image);
    return TCL_OK;
}

static int XAxisCmd(int second, Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int cmd;
    enum commands {
        cmdSetTitle, cmdSetLabels,
        cmdSetLabelStyle, cmdSetIndent,
        cmdSetLinearScale, cmdSetTickLength,
        cmdSetWidth, cmdAddMark,
        cmdAddZone
    };

    static const char *sCmd[] = {
        "settitle", "setlabels",
        "setlabelstyle", "setindent",
        "setlinearscale", "setticklength",
        "setwidth", "addmark",
        "addzone",
        0
    };
    if (chart->type != XYChartType) {
        Tcl_AppendResult(interp, "wrong chart type", 0);
        return TCL_ERROR;
    }
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart command ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], sCmd, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    XAxis *xaxis = second ? chart->xy->xAxis2() : chart->xy->xAxis();

    switch (cmd) {
    case cmdSetTitle:
        xaxis->setTitle(Tcl_GetStringFromObj(objv[4], 0));
        break;

    case cmdSetIndent:{
            int indent;
            if (Tcl_GetIntFromObj(interp, objv[4], &indent) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "indent");
                return TCL_ERROR;
            }
            xaxis->setIndent(indent);
            break;
        }

    case cmdSetWidth:{
            int width;
            if (Tcl_GetIntFromObj(interp, objv[4], &width) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "width");
                return TCL_ERROR;
            }
            xaxis->setWidth(width);
            break;
        }

    case cmdSetLinearScale:{
            double llimit, ulimit;
            double tickinc = 0;

            if (objc < 6 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &llimit) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, objv[5], &ulimit) != TCL_OK ||
                (objc > 6 && Tcl_GetDoubleFromObj(interp, objv[6], &tickinc) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "lowerlimit upperlimit ?tickinc?");
                return TCL_ERROR;
            }
            xaxis->setLinearScale(llimit, ulimit, tickinc);
            break;
        }

    case cmdAddZone:{
            double start, end;
            int color;

            if (objc < 7 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &start) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, objv[5], &end) != TCL_OK || chartColor(interp, objv[6], &color) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "start end color");
                return TCL_ERROR;
            }
            xaxis->addZone(start, end, color);
            break;
        }

    case cmdAddMark:{
            double value;
            int linecolor;
            int linewidth;
            char *text;
            char *align = 0;
            char *font = 0;
            int fontsize = 8;
            int fontcolor = TextColor;
            int fontangle = 0;
            int ontop = true;
            int tickcolor = 0;

            if (objc < 8 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &value) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[5], &linecolor) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[6], &linewidth) != TCL_OK ||
                !(text = Tcl_GetStringFromObj(objv[7], 0)) ||
                (objc > 8 && !(align = Tcl_GetStringFromObj(objv[8], 0))) ||
                (objc > 9 && !(font = Tcl_GetStringFromObj(objv[9], 0))) ||
                (objc > 10 && Tcl_GetIntFromObj(interp, objv[10], &fontsize) != TCL_OK) ||
                (objc > 11 && chartColor(interp, objv[11], &fontcolor) != TCL_OK) ||
                (objc > 12 && Tcl_GetIntFromObj(interp, objv[12], &fontangle) != TCL_OK) ||
                (objc > 13 && Tcl_GetIntFromObj(interp, objv[13], &ontop) != TCL_OK) ||
                (objc > 14 && chartColor(interp, objv[14], &tickcolor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv,
                                 "value linecolor linewidth text ?align? ?font? ?fontsize? ?fontcolor? ?fontangle? ?ontop? ?tickcolor?");
                return TCL_ERROR;
            }
            Mark *mark = xaxis->addMark(value, linecolor, text, font, fontsize);
            mark->setLineWidth(linewidth);
            mark->setMarkColor(linecolor, fontcolor, tickcolor ? tickcolor : linecolor);
            mark->setAlignment(chartAlignment(align, TopCenter));
            mark->setDrawOnTop(ontop);
            mark->setFontAngle(fontangle);
            break;
        }

    case cmdSetLabels:{
            int argc;
            Tcl_Obj **argv;

            if (Tcl_ListObjGetElements(interp, objv[4], &argc, &argv) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "labels");
                return TCL_ERROR;
            }
            for (int i = 0; i < argc; i++)
                xaxis->addLabel(i, Tcl_GetStringFromObj(argv[i], 0));
            break;
        }

    case cmdSetTickLength:{
            int major;
            int minor = 0;

            if (Tcl_GetIntFromObj(interp, objv[4], &major) != TCL_OK ||
                (objc > 5 && Tcl_GetIntFromObj(interp, objv[5], &minor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "major ?minor?");
                return TCL_ERROR;
            }
            xaxis->setTickLength(major, minor);
            break;
        }

    case cmdSetLabelStyle:{
            const char *font;
            int fontsize;
            int fontcolor;
            int fontangle;

            if (objc < 8 ||
                !(font = Tcl_GetStringFromObj(objv[4], 0)) ||
                Tcl_GetIntFromObj(interp, objv[5], &fontsize) != TCL_OK ||
                chartColor(interp, objv[6], &fontcolor) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[7], &fontangle) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "font fontsize fontcolor fontangle");
                return TCL_ERROR;
            }
            xaxis->setLabelStyle(font, fontsize, fontcolor, fontangle);
            break;
        }
    }
    return TCL_OK;
}

static int YAxisCmd(int second, Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int cmd;
    enum commands {
        cmdSetTitle, cmdSetFormat,
        cmdSetLabelStyle, cmdSetTopMargin,
        cmdSetLinearScale, cmdSetAutoScale,
        cmdSetTickDensity, cmdSetLogScale,
        cmdSetTickLength, cmdSetWidth,
        cmdAddMark, cmdAddZone,
        cmdSyncYAxis, cmdSetYAxisOnRight
    };

    static const char *sCmd[] = {
        "settitle", "setformat",
        "setlabelstyle", "settopmargin",
        "setlinearscale", "setautoscale",
        "settickdensity", "setlogscale",
        "setticklength", "setwidth",
        "addmark", "addzone",
        "syncyaxis", "setyaxisonright",
        0
    };
    if (chart->type != XYChartType) {
        Tcl_AppendResult(interp, "wrong chart type", 0);
        return TCL_ERROR;
    }
    if (objc < 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart command ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], sCmd, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    YAxis *yaxis = second ? chart->xy->yAxis2() : chart->xy->yAxis();

    switch (cmd) {

    case cmdSyncYAxis:{
            double slope;
            double intercept = 0;

            if (Tcl_GetDoubleFromObj(interp, objv[4], &slope) != TCL_OK ||
                (objc > 5 && Tcl_GetDoubleFromObj(interp, objv[5], &intercept) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "slope ?intercept?");
                return TCL_ERROR;
            }
            chart->xy->syncYAxis(slope, intercept);
            break;
        }

    case cmdSetYAxisOnRight:{
            int right;
            if (Tcl_GetIntFromObj(interp, objv[4], &right) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "onright");
                return TCL_ERROR;
            }
            chart->xy->setYAxisOnRight(right);
            break;
        }

    case cmdSetTitle:
        yaxis->setTitle(Tcl_GetStringFromObj(objv[4], 0));
        break;

    case cmdSetFormat:
        chart->xy->yAxis()->setLabelFormat(Tcl_GetStringFromObj(objv[4], 0));
        break;

    case cmdSetWidth:{
            int width;
            if (Tcl_GetIntFromObj(interp, objv[4], &width) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "width");
                return TCL_ERROR;
            }
            yaxis->setWidth(width);
            break;
        }

    case cmdAddMark:{
            double value;
            int linecolor;
            int linewidth;
            char *text;
            char *align = 0;
            char *font = 0;
            int fontsize = 8;
            int fontcolor = TextColor;
            int fontangle = 0;
            int ontop = true;
            int tickcolor = 0;

            if (objc < 8 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &value) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[5], &linecolor) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[6], &linewidth) != TCL_OK ||
                !(text = Tcl_GetStringFromObj(objv[7], 0)) ||
                (objc > 8 && !(align = Tcl_GetStringFromObj(objv[8], 0))) ||
                (objc > 9 && !(font = Tcl_GetStringFromObj(objv[9], 0))) ||
                (objc > 10 && Tcl_GetIntFromObj(interp, objv[10], &fontsize) != TCL_OK) ||
                (objc > 11 && chartColor(interp, objv[11], &fontcolor) != TCL_OK) ||
                (objc > 12 && Tcl_GetIntFromObj(interp, objv[12], &fontangle) != TCL_OK) ||
                (objc > 13 && Tcl_GetIntFromObj(interp, objv[13], &ontop) != TCL_OK) ||
                (objc > 14 && chartColor(interp, objv[14], &tickcolor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv,
                                 "value linecolor linewidth text ?align? ?font? ?fontsize? ?fontcolor? ?fontangle? ?ontop? ?tickcolor?");
                return TCL_ERROR;
            }
            Mark *mark = yaxis->addMark(value, linecolor, text, font, fontsize);
            mark->setLineWidth(linewidth);
            mark->setMarkColor(linecolor, fontcolor, tickcolor ? tickcolor : linecolor);
            mark->setAlignment(chartAlignment(align, TopCenter));
            mark->setDrawOnTop(ontop);
            break;
        }

    case cmdSetLabelStyle:{
            const char *font;
            int fontsize;
            int fontcolor;
            int fontangle;

            if (objc < 8 ||
                !(font = Tcl_GetStringFromObj(objv[4], 0)) ||
                Tcl_GetIntFromObj(interp, objv[5], &fontsize) != TCL_OK ||
                chartColor(interp, objv[6], &fontcolor) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[7], &fontangle) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "font fontsize fontcolor fontangle");
                return TCL_ERROR;
            }
            yaxis->setLabelStyle(font, fontsize, fontcolor, fontangle);
            break;
        }

    case cmdSetTopMargin:{
            int margin;
            if (Tcl_GetIntFromObj(interp, objv[4], &margin) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "margin");
                return TCL_ERROR;
            }
            yaxis->setTopMargin(margin);
            break;
        }

    case cmdSetTickDensity:{
            int density;
            if (Tcl_GetIntFromObj(interp, objv[4], &density) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "density");
                return TCL_ERROR;
            }
            yaxis->setTickDensity(density);
            break;
        }

    case cmdSetTickLength:{
            int major;
            int minor = 0;

            if (Tcl_GetIntFromObj(interp, objv[4], &major) != TCL_OK ||
                (objc > 5 && Tcl_GetIntFromObj(interp, objv[5], &minor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "major ?minor?");
                return TCL_ERROR;
            }
            yaxis->setTickLength(major, minor);
            break;
        }

    case cmdSetLinearScale:{
            double llimit, ulimit;
            double tickinc = 0;

            if (objc < 6 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &llimit) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, objv[5], &ulimit) != TCL_OK ||
                (objc > 6 && Tcl_GetDoubleFromObj(interp, objv[6], &tickinc) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "lowerlimit upperlimit ?tickinc?");
                return TCL_ERROR;
            }
            yaxis->setLinearScale(llimit, ulimit, tickinc);
            break;
        }

    case cmdAddZone:{
            double start, end;
            int color;

            if (objc < 7 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &start) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, objv[5], &end) != TCL_OK || chartColor(interp, objv[6], &color) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "start end color");
                return TCL_ERROR;
            }
            yaxis->addZone(start, end, color);
            break;
        }

    case cmdSetLogScale:{
            double llimit, ulimit;
            double tickinc = 0;

            if (objc < 6 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &llimit) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, objv[5], &ulimit) != TCL_OK ||
                (objc > 6 && Tcl_GetDoubleFromObj(interp, objv[6], &tickinc) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "lowerlimit upperlimit ?tickinc?");
                return TCL_ERROR;
            }
            yaxis->setLogScale(llimit, ulimit, tickinc);
            break;
        }

    case cmdSetAutoScale:{
            double top;
            double bottom = 0;
            double zeroaffinity = 0.8;

            if (objc < 5 ||
                Tcl_GetDoubleFromObj(interp, objv[4], &top) != TCL_OK ||
                (objc > 5 && Tcl_GetDoubleFromObj(interp, objv[5], &bottom) != TCL_OK) ||
                (objc > 6 && Tcl_GetDoubleFromObj(interp, objv[6], &zeroaffinity) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "topextension ?bottomextension? ?zeroaffinity?");
                return TCL_ERROR;
            }
            yaxis->setAutoScale(top, bottom, zeroaffinity);
            break;
        }

    }
    return TCL_OK;
}

static int LayerCmd(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int cmd, layer, datasetID;
    DataSet *dataset;

    enum commands {
        cmdCreate, cmdSetLineWidth,
        cmdSetDataSymbol, cmdDataSet,
        cmdSetDataColor, cmdSet3D,
        cmdSetDepth, cmdSetDataCombineMethod,
        cmdSetBarGap, cmdSetGapColor,
        cmdSetBorderColor, cmdSetDataLabelStyle,
        cmdSetAggregateLabelStyle
    };

    static const char *sCmd[] = {
        "create", "setlinewidth",
        "setdatasymbol", "dataset",
        "setdatacolor", "set3d",
        "setdepth", "setdatacombinemethod",
        "setbargap", "setgapcolor",
        "setbordercolor", "setdatalabelstyle",
        "setaggregatelabelstyle",
        0
    };
    if (chart->type != XYChartType) {
        Tcl_AppendResult(interp, "wrong chart type", 0);
        return TCL_ERROR;
    }
    if (objc < 5) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart cmd arg1 arg2");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], sCmd, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    if (cmd > cmdCreate) {
        if (Tcl_GetIntFromObj(interp, objv[4], &layer) != TCL_OK)
            return TCL_ERROR;
        if (layer < 0 || layer >= MAX_LAYERS || !chart->layers[layer].layer) {
            Tcl_AppendResult(interp, "wrong layer #", 0);
            return TCL_ERROR;
        }
    } else {
        for (layer = 0; layer < MAX_LAYERS && chart->layers[layer].layer; layer++);
        if (layer == MAX_LAYERS) {
            Tcl_AppendResult(interp, "no more available layer slots left", 0);
            return TCL_ERROR;
        }
    }

    switch (cmd) {
    case cmdCreate:{
            int argc;
            Tcl_Obj **argv;
            char *name = 0;
            int color = -1;

            if (objc < 6 || Tcl_ListObjGetElements(interp, objv[5], &argc, &argv) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "type data ?name? ?color?");
                return TCL_ERROR;
            }

            double *data = (double *) ns_malloc(argc * sizeof(double));
            for (int i = 0; i < argc; i++)
                data[i] = atof(Tcl_GetStringFromObj(argv[i], 0));

            char *type = Tcl_GetStringFromObj(objv[4], 0);

            if (!strcmp("line", type)) {
                if ((objc > 6 && !(name = Tcl_GetStringFromObj(objv[6], 0))) ||
                    (objc > 7 && chartColor(interp, objv[7], &color) != TCL_OK)) {
                    Tcl_WrongNumArgs(interp, 4, objv, "type data ?name? ?color?");
                    ns_free(data);
                    return TCL_ERROR;
                }
                chart->layers[layer].line = chart->xy->addLineLayer(argc, data, color, name);
                chart->layers[layer].layer = chart->layers[layer].line;
                chart->layers[layer].type = LineType;
            } else if (!strcmp("bar", type)) {
                int colorc = 0, namec = 0;
                Tcl_Obj **colorv, **namev;
                int *colors = 0;
                char **names = 0;

                if (objc > 6) {
                    if (Tcl_ListObjGetElements(interp, objv[6], &namec, &namev) != TCL_OK || namec != argc)
                        name = Tcl_GetStringFromObj(objv[6], 0);
                    else {
                        names = (char **) ns_malloc(namec * sizeof(char *));
                        for (int i = 0; i < namec; i++)
                            names[i] = Tcl_GetStringFromObj(namev[i], 0);
                    }
                }
                if (objc > 7) {
                    if (Tcl_ListObjGetElements(interp, objv[7], &colorc, &colorv) != TCL_OK ||
                        (colorc > 1 && colorc != argc)) {
                        Tcl_WrongNumArgs(interp, 4, objv, "type data ?names? ?colors?, invalid number of items in colors");
                        ns_free(data);
                        return TCL_ERROR;
                    }
                    if (colorc == 1)
                        chartColor(interp, objv[7], &color);
                    else {
                        colors = (int *) ns_malloc(colorc * sizeof(int));
                        for (int i = 0; i < colorc; i++)
                            Tcl_GetIntFromObj(interp, colorv[i], &colors[i]);
                    }
                }

                if (colors || names)
                    chart->layers[layer].bar = chart->xy->addBarLayer(argc, data, colors, names);
                else if (color == -1 && name == 0)
                    chart->layers[layer].bar = chart->xy->addBarLayer(DoubleArray(data, argc), IntArray(0, 0));
                else
                    chart->layers[layer].bar = chart->xy->addBarLayer(argc, data, color, name);

                chart->layers[layer].layer = chart->layers[layer].bar;
                chart->layers[layer].type = BarType;
                if (objc > 6 && namec != argc)
                    chart->layers[layer].layer->getDataSet(0)->setDataName(name);
                ns_free(colors);
                ns_free(names);
            } else if (!strcmp("area", type)) {
                if ((objc > 6 && !(name = Tcl_GetStringFromObj(objv[6], 0))) ||
                    (objc > 7 && chartColor(interp, objv[7], &color) != TCL_OK)) {
                    Tcl_WrongNumArgs(interp, 4, objv, "type data ?name? ?color?");
                    return TCL_ERROR;
                }
                chart->layers[layer].layer = chart->xy->addAreaLayer(argc, data, color, name);
                chart->layers[layer].type = AreaType;
            } else if (!strcmp("trend", type)) {
                if ((objc > 6 && !(name = Tcl_GetStringFromObj(objv[6], 0))) ||
                    (objc > 7 && chartColor(interp, objv[7], &color) != TCL_OK)) {
                    Tcl_WrongNumArgs(interp, 4, objv, "type data ?name? ?color?");
                    return TCL_ERROR;
                }
                chart->layers[layer].trend = chart->xy->addTrendLayer(DoubleArray(data, argc), color, name);
                chart->layers[layer].layer = chart->layers[layer].trend;
                chart->layers[layer].type = TrendType;
            } else {
                Tcl_AppendResult(interp, "wrong layer type: should be one of line bar scatter area trend hloc candlestick",
                                 0);
                ns_free(data);
                return TCL_ERROR;
            }
            ns_free(data);
            Tcl_SetObjResult(interp, Tcl_NewIntObj(layer));
            break;
        }

    case cmdSet3D:
        chart->layers[layer].layer->set3D();
        break;

    case cmdSetLineWidth:{
            int width;
            if (Tcl_GetIntFromObj(interp, objv[5], &width) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer width");
                return TCL_ERROR;
            }
            chart->layers[layer].layer->setLineWidth(width);
            break;
        }

    case cmdSetDepth:{
            int depth;
            int gap = -1;

            if (objc < 6 ||
                Tcl_GetIntFromObj(interp, objv[5], &depth) != TCL_OK ||
                (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &gap) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer depth ?gap?");
                return TCL_ERROR;
            }
            chart->layers[layer].layer->set3D(depth, gap);
            break;
        }

    case cmdSetBarGap:{
            double bargap;
            double subbargap = 0.2;

            if (objc < 6 ||
                Tcl_GetDoubleFromObj(interp, objv[5], &bargap) != TCL_OK ||
                (objc > 6 && Tcl_GetDoubleFromObj(interp, objv[6], &subbargap) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer bargap ?subbargap?");
                return TCL_ERROR;
            }
            if (chart->layers[layer].type != BarType)
                break;
            chart->layers[layer].bar->setBarGap(bargap, subbargap);
            break;
        }

    case cmdSetGapColor:{
            int color;
            int width = -1;

            if (objc < 6 ||
                Tcl_GetIntFromObj(interp, objv[5], &color) != TCL_OK ||
                (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &width) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer color ?width?");
                return TCL_ERROR;
            }
            if (chart->layers[layer].type != LineType)
                break;
            chart->layers[layer].line->setGapColor(color, width);
            break;
        }

    case cmdSetBorderColor:{
            int color;
            int border = 0;

            if (objc < 6 ||
                chartColor(interp, objv[5], &color) != TCL_OK ||
                (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &border) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer color ?raiseeffect?");
                return TCL_ERROR;
            }
            chart->layers[layer].layer->setBorderColor(color, border);
            break;
        }

    case cmdSetDataCombineMethod:{
            int method;
            char *name;
            char *DataCombineMethod[] = { (char*)"Overlay", (char*)"Stack", (char*)"Depth", (char*)"Side", 0 };

            if (objc < 6 || !(name = Tcl_GetStringFromObj(objv[5], 0))) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer datacombinemethod");
                return TCL_ERROR;
            }
            for (method = 0; DataCombineMethod[method]; method++)
                if (!strcasecmp(name, DataCombineMethod[method]))
                    break;
            chart->layers[layer].layer->setDataCombineMethod(method);
            break;
        }
    case cmdDataSet:{
            char *name = 0;
            int color = -1;
            int argc;
            Tcl_Obj **argv;

            if (Tcl_ListObjGetElements(interp, objv[5], &argc, &argv) != TCL_OK ||
                (objc > 6 && !(name = Tcl_GetStringFromObj(objv[6], 0))) ||
                (objc > 7 && chartColor(interp, objv[7], &color) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer data ?name? ?color?");
                return TCL_ERROR;
            }
            double *data = (double *) ns_malloc(argc * sizeof(double));
            for (int i = 0; i < argc; i++)
                data[i] = atof(Tcl_GetStringFromObj(argv[i], 0));

            chart->layers[layer].layer->addDataSet(argc, data, color, name);
            ns_free(data);
            break;
        }

    case cmdSetDataSymbol:{
            int symbol;
            char *symbolName;
            int size = 5;
            int fillcolor = -1;
            int edgecolor = -1;

            if (objc < 7 ||
                Tcl_GetIntFromObj(interp, objv[5], &datasetID) != TCL_OK ||
                !(dataset = chart->layers[layer].layer->getDataSet(datasetID)) ||
                !(symbolName = Tcl_GetStringFromObj(objv[6], 0)) ||
                (objc > 7 && Tcl_GetIntFromObj(interp, objv[7], &size) != TCL_OK) ||
                (objc > 8 && chartColor(interp, objv[8], &fillcolor) != TCL_OK) ||
                (objc > 9 && chartColor(interp, objv[9], &edgecolor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer #dataset symbol ?size? ?fillcolor? ?edgecolor?");
                return TCL_ERROR;
            }
            for (symbol = 0; chartSymbolTypes[symbol]; symbol++)
                if (!strcasecmp(symbolName, chartSymbolTypes[symbol]))
                    break;
            if (!chartSymbolTypes[symbol])
                dataset->setDataSymbol(symbolName);
            else
                dataset->setDataSymbol((SymbolType) symbol, size, fillcolor, edgecolor);
            break;
        }

    case cmdSetDataColor:{
            int datacolor;
            int edgecolor = -1;
            int shadowcolor = -1;
            int shadowedgecolor = -1;

            if (objc < 7 ||
                Tcl_GetIntFromObj(interp, objv[5], &datasetID) != TCL_OK ||
                !(dataset = chart->layers[layer].layer->getDataSet(datasetID)) ||
                chartColor(interp, objv[6], &datacolor) != TCL_OK ||
                (objc > 7 && chartColor(interp, objv[7], &edgecolor) != TCL_OK) ||
                (objc > 8 && chartColor(interp, objv[8], &shadowcolor) != TCL_OK) ||
                (objc > 9 && chartColor(interp, objv[9], &shadowedgecolor) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "#layer #dataset datacolor ?edgecolor? ?shadowcolor? ?shadowedgecolor?");
                return TCL_ERROR;
            }
            dataset->setDataColor(datacolor, edgecolor, shadowcolor, shadowedgecolor);
            break;
        }

    case cmdSetDataLabelStyle:{
            const char *font;
            int fontsize = 8;
            int fontcolor = TextColor;
            int fontangle = 0;

            if (!(font = Tcl_GetStringFromObj(objv[5], 0)) ||
                (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &fontsize) != TCL_OK) ||
                (objc > 7 && chartColor(interp, objv[7], &fontcolor) != TCL_OK) ||
                (objc > 8 && Tcl_GetIntFromObj(interp, objv[8], &fontangle) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "font ?fontsize? ?fontcolor? ?fontangle?");
                return TCL_ERROR;
            }
            chart->layers[layer].layer->setDataLabelStyle(font, fontsize, fontcolor, fontangle);
            break;
        }

    case cmdSetAggregateLabelStyle:{
            const char *font;
            int fontsize = 8;
            int fontcolor = TextColor;
            int fontangle = 0;

            if (!(font = Tcl_GetStringFromObj(objv[5], 0)) ||
                (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &fontsize) != TCL_OK) ||
                (objc > 7 && chartColor(interp, objv[7], &fontcolor) != TCL_OK) ||
                (objc > 8 && Tcl_GetIntFromObj(interp, objv[8], &fontangle) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "font ?fontsize? ?fontcolor? ?fontangle?");
                return TCL_ERROR;
            }
            chart->layers[layer].layer->setAggregateLabelStyle(font, fontsize, fontcolor, fontangle);
            break;
        }
    }
    return TCL_OK;
}

static int dashLineColor(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int color;
    int pattern;

    if (objc < 5 || chartColor(interp, objv[3], &color) != TCL_OK || Tcl_GetIntFromObj(interp, objv[4], &pattern) != TCL_OK) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart color pattern");
        return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(chart->chart->dashLineColor(color, pattern)));
    return TCL_OK;
}

static int patternColor(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int i, argc;
    int width = 0;
    int height = 0;
    int startx = 0;
    int starty = 0;
    Tcl_Obj **argv;

    if (objc < 4 ||
        Tcl_ListObjGetElements(interp, objv[3], &argc, &argv) != TCL_OK ||
        (objc > 4 && Tcl_GetIntFromObj(interp, objv[4], &width) != TCL_OK) ||
        (objc > 5 && Tcl_GetIntFromObj(interp, objv[5], &height) != TCL_OK) ||
        (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &startx) != TCL_OK) ||
        (objc > 7 && Tcl_GetIntFromObj(interp, objv[7], &startx) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart pattern width height ?startx? ?starty?");
        return TCL_ERROR;
    }
    // Check if it is filename
    if (argc == 1 && Tcl_GetIntFromObj(interp, argv[0], &i) != TCL_OK) {
        Tcl_SetObjResult(interp,
                         Tcl_NewIntObj(chart->chart->patternColor(Tcl_GetStringFromObj(argv[0], 0), startx, starty)));
        return TCL_OK;
    }
    if (width * height != argc) {
        Tcl_AppendResult(interp, "wrong width/height for the pattern bitmap", 0);
        return TCL_ERROR;
    }
    int *pattern = (int *) ns_malloc(argc * sizeof(int));
    for (i = 0; i < argc; i++)
        Tcl_GetIntFromObj(interp, argv[i], &pattern[i]);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(chart->chart->patternColor(pattern, width, height, startx, starty)));
    ns_free(pattern);
    return TCL_OK;
}

static int gradientColor(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int *array, argc;
    char *name = 0;
    Tcl_Obj **argv;
    double angle = 90;
    double scale = 1;
    int startx = 0;
    int starty = 0;

    if (objc < 4 ||
        Tcl_ListObjGetElements(interp, objv[3], &argc, &argv) != TCL_OK ||
        (objc > 4 && Tcl_GetDoubleFromObj(interp, objv[4], &angle) != TCL_OK) ||
        (objc > 5 && Tcl_GetDoubleFromObj(interp, objv[5], &scale) != TCL_OK) ||
        (objc > 6 && Tcl_GetIntFromObj(interp, objv[6], &startx) != TCL_OK) ||
        (objc > 7 && Tcl_GetIntFromObj(interp, objv[7], &starty) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart array ?angle? ?scale? ?startx? ?starty?");
        return TCL_ERROR;
    }
    // Check if it is name
    if (argc == 1 && (name = Tcl_GetStringFromObj(argv[0], 0)) &&
        ((!strcasecmp("goldGradient", name) && (array = (int *) goldGradient)) ||
         (!strcasecmp("silverGradient", name) && (array = (int *) silverGradient)) ||
         (!strcasecmp("redMetalGradient", name) && (array = (int *) redMetalGradient)) ||
         (!strcasecmp("blueMetalGradient", name) && (array = (int *) blueMetalGradient)) ||
         (!strcasecmp("greenMetalGradient", name) && (array = (int *) greenMetalGradient)))) {
        Tcl_SetObjResult(interp, Tcl_NewIntObj(chart->chart->gradientColor(array, angle, scale, startx, starty)));
        return TCL_OK;
    } else {
        array = (int *) ns_malloc(argc * sizeof(int));
        for (int i = 0; i < argc; i++)
            Tcl_GetIntFromObj(interp, argv[i], &array[i]);
        Tcl_SetObjResult(interp, Tcl_NewIntObj(chart->chart->gradientColor(array, angle, scale, startx, starty)));
        ns_free(array);
    }
    return TCL_OK;
}

static int addText(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int x, y;
    const char *text;
    const char *font = 0;
    const char *align = 0;
    int fontsize = 8;
    int fontcolor = TextColor;
    int vertical = 0;
    double angle = 0;

    if (objc < 6 ||
        Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK ||
        Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK ||
        !(text = Tcl_GetStringFromObj(objv[5], 0)) ||
        (objc > 6 && !(font = Tcl_GetStringFromObj(objv[6], 0))) ||
        (objc > 7 && Tcl_GetIntFromObj(interp, objv[7], &fontsize) != TCL_OK) ||
        (objc > 8 && chartColor(interp, objv[8], &fontcolor) != TCL_OK) ||
        (objc > 9 && !(align = Tcl_GetStringFromObj(objv[9], 0))) ||
        (objc > 10 && Tcl_GetDoubleFromObj(interp, objv[10], &angle) != TCL_OK) ||
        (objc > 11 && Tcl_GetIntFromObj(interp, objv[11], &vertical) != TCL_OK)) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart x y text ?font? ?fontsize? ?fontcolor? ?alignment? ?angle? ?vertical?");
        return TCL_ERROR;
    }
    chart->chart->addText(x, y, text, font, fontsize, fontcolor, chartAlignment(align, TopLeft), angle, vertical);
    return TCL_OK;
}

static int PieCmd(Ns_Chart * chart, int objc, Tcl_Obj * CONST objv[], Tcl_Interp * interp)
{
    int cmd;
    enum commands {
        cmdSetData, cmdSet3D,
        cmdSetPieSize
    };

    static const char *sCmd[] = {
        "setdata", "set3d",
        "setpiesize",
        0
    };
    if (chart->type != PieChartType) {
        Tcl_AppendResult(interp, "wrong chart type", 0);
        return TCL_ERROR;
    }
    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "#chart cmd");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], sCmd, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    switch (cmd) {
    case cmdSetData:{
            char **labels = 0;
            int argc, labelc;
            Tcl_Obj **argv, **labelv;

            if (objc < 5 ||
                Tcl_ListObjGetElements(interp, objv[4], &argc, &argv) != TCL_OK ||
                (objc > 5 && Tcl_ListObjGetElements(interp, objv[5], &labelc, &labelv) != TCL_OK)) {
                Tcl_WrongNumArgs(interp, 4, objv, "data ?labels?");
                return TCL_ERROR;
            }
            double *data = (double *) ns_malloc(argc * sizeof(double));
            for (int i = 0; i < argc; i++)
                Tcl_GetDoubleFromObj(interp, argv[i], &data[i]);

            if (labelc > 0) {
                labels = (char **) ns_malloc(labelc * sizeof(char *));
                for (int i = 0; i < labelc; i++)
                    labels[i] = Tcl_GetStringFromObj(labelv[i], 0);
            }
            chart->pie->setData(argc, data, labels);
            ns_free(data);
            ns_free(labels);
        }

    case cmdSet3D:
        chart->pie->set3D();
        break;

    case cmdSetPieSize:{
            int x, y, r;

            if (objc < 7 ||
                Tcl_GetIntFromObj(interp, objv[4], &x) != TCL_OK ||
                Tcl_GetIntFromObj(interp, objv[5], &y) != TCL_OK || Tcl_GetIntFromObj(interp, objv[6], &r) != TCL_OK) {
                Tcl_WrongNumArgs(interp, 4, objv, "x y r");
                return TCL_ERROR;
            }
            chart->pie->setPieSize(x, y, r);
        }
    }
    return TCL_OK;
}

/*
 *  ns_chartdir implementation
 */

static int ChartCmd(ClientData arg, Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    int i, cmd;
    Ns_Chart *chart = 0;

    enum commands {
        cmdGc, cmdCharts,
        cmdVersion,
        cmdNoValue, cmdTransparentColor,
        cmdPaletteColor, cmdLineColor,
        cmdTextColor, cmdDataColor,
        cmdSameAsMainColor, cmdBackgroundColor,
        cmdCreate, cmdSetBackground,
        cmdSetPlotArea, cmdAddLegend,
        cmdAddTitle, cmdSetSize,
        cmdSetBgImage, cmdSetWallpaper,
        cmdYAxis, cmdXAxis,
        cmdYAxis2, cmdXAxis2,
        cmdLayer, cmdDashLineColor,
        cmdPatternColor, cmdGradientColor,
        cmdAddText, cmdSetColors,
        cmdPie,
        cmdSave, cmdDestroy,
        cmdImage, cmdReturn
    };

    static const char *sCmd[] = {
        "gc", "charts",
        "version",
        "novalue", "transparentcolor",
        "palettecolor", "linecolor",
        "textcolor", "datacolor",
        "sameasmaincolor", "backgroundcolor",
        "create", "setbackground",
        "setplotarea", "addlegend",
        "addtitle", "setsize",
        "setbgimage", "setwallpaper",
        "yaxis", "xaxis",
        "yaxis2", "xaxis2",
        "layer", "dashlinecolor",
        "patterncolor", "gradientcolor",
        "addtext", "setcolors",
        "pie",
        "save", "destroy",
        "image", "return",
        0
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], sCmd, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    if (cmd > cmdCreate) {
        if (objc < 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "command #chart ...");
            return TCL_ERROR;
        }
        if (Tcl_GetIntFromObj(interp, objv[2], &i) != TCL_OK)
            return TCL_ERROR;
        if (!(chart = getChart(i))) {
            Tcl_AppendResult(interp, "Invalid or expired chart object", 0);
            return TCL_ERROR;
        }
        chart->access_time = time(0);
    }

    switch (cmd) {
    case cmdVersion:
        Tcl_AppendResult(interp, "ns_chartdir ", _VERSION, 0);
        break;

    case cmdGc:
        ChartGC(0);
        break;

    case cmdCharts:{
            // Lists charts in memory
            Tcl_Obj *list = Tcl_NewListObj(0, 0);
            Ns_MutexLock(&chartMutex);
            for (chart = chartList; chart; chart = chart->next) {
                Tcl_ListObjAppendElement(interp, list, Tcl_NewIntObj(chart->id));
                Tcl_ListObjAppendElement(interp, list, Tcl_NewIntObj(chart->access_time));
            }
            Ns_MutexUnlock(&chartMutex);
            Tcl_SetObjResult(interp, list);
            return TCL_OK;
        }

    case cmdCreate:
        if (!createChart(objc, objv, interp))
            return TCL_ERROR;
        break;

    case cmdSetBackground:
        return setBackground(chart, objc, objv, interp);

    case cmdSetPlotArea:
        return setPlotArea(chart, objc, objv, interp);

    case cmdAddLegend:
        return addLegend(chart, objc, objv, interp);

    case cmdAddTitle:
        return addTitle(chart, objc, objv, interp);

    case cmdAddText:
        return addText(chart, objc, objv, interp);

    case cmdSetSize:
        return setSize(chart, objc, objv, interp);

    case cmdPie:
        return PieCmd(chart, objc, objv, interp);

    case cmdSetColors:
        return setColors(chart, objc, objv, interp);

    case cmdSetBgImage:
        return setBgImage(chart, objc, objv, interp);

    case cmdSetWallpaper:
        return setWallpaper(chart, objc, objv, interp);

    case cmdYAxis:
        return YAxisCmd(0, chart, objc, objv, interp);

    case cmdXAxis:
        return XAxisCmd(0, chart, objc, objv, interp);

    case cmdYAxis2:
        return YAxisCmd(1, chart, objc, objv, interp);

    case cmdXAxis2:
        return XAxisCmd(1, chart, objc, objv, interp);

    case cmdLayer:
        return LayerCmd(chart, objc, objv, interp);

    case cmdSave:
        chart->chart->makeChart(Tcl_GetStringFromObj(objv[3], 0));
        break;

    case cmdImage:{
            MemBlock mem = chart->chart->makeChart(PNG);
            Tcl_Obj *result = Tcl_NewByteArrayObj((unsigned char *) mem.data, mem.len);
            if (result == NULL)
                return TCL_ERROR;
            Tcl_SetObjResult(interp, result);
            break;
        }

    case cmdReturn:{
            MemBlock mem = chart->chart->makeChart(PNG);
            Ns_Conn *conn = Ns_TclGetConn(interp);
            if (conn == NULL) {
                Tcl_AppendResult(interp, "no connection", NULL);
                return TCL_ERROR;
            }
            int status = Ns_ConnReturnData(conn, 200, (char *) mem.data, mem.len, "image/png");
            Tcl_AppendResult(interp, status == NS_OK ? "1" : "0", NULL);
            break;
        }

    case cmdDestroy:
        freeChart(chart, 1);
        break;

    case cmdNoValue:
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(NoValue));
        break;

    case cmdTransparentColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(Transparent));
        break;

    case cmdPaletteColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(Palette));
        break;

    case cmdBackgroundColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(BackgroundColor));
        break;

    case cmdTextColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(TextColor));
        break;

    case cmdLineColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(LineColor));
        break;

    case cmdDataColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(DataColor));
        break;

    case cmdSameAsMainColor:
        Tcl_SetObjResult(interp, Tcl_NewIntObj(SameAsMainColor));
        break;

    case cmdDashLineColor:
        return dashLineColor(chart, objc, objv, interp);

    case cmdPatternColor:
        return patternColor(chart, objc, objv, interp);

    case cmdGradientColor:
        return gradientColor(chart, objc, objv, interp);
    }
    return TCL_OK;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * indent-tabs-mode: nil
 * End:
 */
