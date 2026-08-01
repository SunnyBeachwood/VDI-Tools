/* VDI-Tools UI localization */
#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <windows.h>

#define VDI_LANGUAGE_AUTO  (-1)
#define VDI_LANGUAGE_EN     0
#define VDI_LANGUAGE_ZH_CN  1
#define VDI_LANGUAGE_DE     2
#define VDI_LANGUAGE_JA     3
#define VDI_LANGUAGE_FR     4
#define VDI_LANGUAGE_ES     5
#define VDI_LANGUAGE_ZH_TW  6
#define VDI_LANGUAGE_KO     7
#define VDI_LANGUAGE_RU     8
#define VDI_LANGUAGE_PT_BR  9

int  Localization_ResolveLanguage(int preference);
void Localization_PopulateLanguageCombo(HWND hCombo, int preference);
int  Localization_GetLanguageComboValue(HWND hCombo);
void Localization_ApplyMainDialog(HWND hDlg, int preference);

#endif
