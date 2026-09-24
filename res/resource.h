#pragma once

#define IDI_APP_ICON                    101
#define IDI_DOC_MD                      105
#define IDC_PLUMA                       102
#define IDR_MAIN_MENU                   103
#define IDR_ACCELERATOR                 104

// Command IDs
#define IDM_FILE_NEW                    40001
#define IDM_FILE_OPEN                   40002
#define IDM_FILE_SAVE                   40003
#define IDM_FILE_SAVEAS                 40004
#define IDM_FILE_EXPORT_HTML            40006
#define IDM_FILE_EXPORT_PDF             40007
#define IDM_FILE_EXIT                   40005

#define IDM_EDIT_UNDO                   40011
#define IDM_EDIT_REDO                   40012
#define IDM_EDIT_CUT                    40013
#define IDM_EDIT_COPY                   40014
#define IDM_EDIT_PASTE                  40015
#define IDM_EDIT_FIND                   40016
#define IDM_EDIT_REPLACE                40017
#define IDM_EDIT_GOTO                   40018
#define IDM_EDIT_WRAP                   40019
#define IDM_EDIT_SELECTALL              40020

#define IDM_VIEW_EDITOR_ONLY            40031
#define IDM_VIEW_SPLIT                  40032
#define IDM_VIEW_PREVIEW_ONLY           40033
#define IDM_VIEW_OUTLINE                40034
#define IDM_VIEW_THEME_SYSTEM           40061
#define IDM_VIEW_THEME_DARK             40062
#define IDM_VIEW_THEME_LIGHT            40063

#define IDM_FORMAT_BOLD                 40051
#define IDM_FORMAT_ITALIC               40052
#define IDM_FORMAT_CODE                 40053
#define IDM_FORMAT_STRIKE               40054
#define IDM_FORMAT_LINK                 40055

#define IDM_VIEW_OUTLINE_PANEL          40035
#define IDM_VIEW_STATUSBAR              40036

#define IDM_SETTINGS_PREFERENCES        40071
#define IDM_SETTINGS_DEFAULT_EDITOR     40072

#define IDM_HELP_ABOUT                  40041
#define IDM_HELP_CHECK_UPDATES          40042

// Preferences dialog
#ifndef IDC_STATIC
#define IDC_STATIC                      (-1)
#endif
#define IDD_SETTINGS                    200
#define IDC_SET_THEME                   2001
#define IDC_SET_FONT                    2002
#define IDC_SET_FONTSIZE                2003
#define IDC_SET_ZOOM                    2004
#define IDC_SET_WORDWRAP                2005
#define IDC_SET_LINENUMBERS             2006
#define IDC_SET_CURRENTLINE             2007
#define IDC_SET_USETABS                 2008
#define IDC_SET_TABWIDTH                2009
#define IDC_SET_EOL                     2010
#define IDC_SET_STARTVIEW               2011
#define IDC_SET_OUTLINE                 2012
#define IDC_SET_STATUSBAR               2013
#define IDC_SET_SYNCSCROLL              2014
#define IDC_SET_REMEMBERWINDOW          2015
#define IDC_SET_REOPENLAST              2016
#define IDC_SET_PDFPAGE                 2017
#define IDC_SET_PDFMARGIN               2018
#define IDC_SET_HTMLTHEME               2019
#define IDC_SET_ASSOC_STATUS            2020
#define IDC_SET_ASSOC_BUTTON            2021
#define IDC_SET_RESET                   2022
#define IDC_SET_APPLY                   2023
#define IDC_SET_CHECKUPDATES            2024
#define IDC_SET_HEADER_APPEARANCE       2031
#define IDC_SET_HEADER_EDITOR           2032
#define IDC_SET_HEADER_VIEW             2033
#define IDC_SET_HEADER_EXPORT           2034
#define IDC_SET_HEADER_WINDOWS          2035

// String IDs
#define IDS_APP_TITLE                   1001
#define IDS_UNTITLED                    1002
#define IDS_SAVE_CHANGES_PROMPT         1003
#define IDS_FILTER_MARKDOWN             1004
#define IDS_FILTER_ALL                  1005
#define IDS_FILTER_HTML                 1006
#define IDS_FILTER_PDF                  1007
