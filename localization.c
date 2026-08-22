/* VDI-Tools UI localization */
#include <windows.h>
#include <stdio.h>
#include "localization.h"

#define IDD_LANGUAGE_COMBO     217
#define IDD_GROUP_FILES        400
#define IDD_LABEL_SOURCE       401
#define IDD_LABEL_DESTINATION  402
#define IDD_GROUP_SOURCEINFO   403
#define IDD_LABEL_VALIDATION   404
#define IDD_LABEL_DRIVESIZE    405
#define IDD_LABEL_FILESYSTEM   406
#define IDD_LABEL_ENCRYPTION   407
#define IDD_GROUP_OPTIONS      408
#define IDD_GROUP_ABOUT        409
#define IDD_UUID_HINT          410
#define IDD_LABEL_LANGUAGE     411

typedef struct {
   const WCHAR *title, *files, *source, *destination, *browse;
   const WCHAR *sourceInfo, *validation, *driveSize, *fileSystem, *encryption;
   const WCHAR *options, *newVdi, *inPlace, *newUuid, *keepUuid, *increaseSize;
   const WCHAR *increasePartition, *compact, *about, *partitionInfo, *headerInfo;
   const WCHAR *sectorViewer, *proceed, *exit, *uuidHint, *language, *aboutText;
} UI_TEXT;

static const WCHAR *const languageNames[] = {
   L"Default (system language)", L"English (en)", L"\x7B80\x4F53\x4E2D\x6587 (zh-CN)",
   L"Deutsch (de)", L"\x65E5\x672C\x8A9E (ja)", L"Fran\x00E7" L"ais (fr)",
   L"Espa\x00F1ol (es)", L"\x7E41\x9AD4\x4E2D\x6587 (zh-TW)", L"\xD55C\xAD6D\xC5B4 (ko)",
   L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439 (ru)", L"Portugu\x00EAs do Brasil (pt-BR)"
};

static const UI_TEXT uiText[] = {
 {L"VDI-Tools 6.00",L" Files ",L"Source:",L"Destination:",L"Browse...",L" Source Drive Information ",L"Validation result:",L"Drive size:",L"File system:",L"Encryption:",L" Options ",L"Create a new VDI",L"Modify source VDI in place",L"Generate new UUID",L"Keep old UUID",L"Increase virtual drive size to",L"Increase partition size",L"Compact unused blocks",L" About ",L"Partition &Info...",L"&Header info...",L"&Sector viewer...",L"&Proceed",L"&Exit",L"(Keep this checked if you don't know what a UUID is!)",L"Language:",L"VDI-Tools 6.00. Based on CloneVDI \x00A9 2010 Don Milne."},
 {L"VDI-Tools 6.00",L" \x6587\x4EF6 ",L"\x6E90\x6587\x4EF6:",L"\x76EE\x6807\x6587\x4EF6:",L"\x6D4F\x89C8...",L" \x6E90\x78C1\x76D8\x4FE1\x606F ",L"\x9A8C\x8BC1\x7ED3\x679C:",L"\x78C1\x76D8\x5BB9\x91CF:",L"\x6587\x4EF6\x7CFB\x7EDF:",L"\x52A0\x5BC6:",L" \x9009\x9879 ",L"\x521B\x5EFA\x65B0\x7684 VDI",L"\x539F\x5730\x4FEE\x6539\x6E90 VDI",L"\x751F\x6210\x65B0 UUID",L"\x4FDD\x7559\x539F UUID",L"\x5C06\x865A\x62DF\x78C1\x76D8\x5BB9\x91CF\x589E\x5927\x81F3",L"\x6269\x5C55\x5206\x533A\x5BB9\x91CF",L"\x538B\x7F29\x672A\x4F7F\x7528\x5757",L" \x5173\x4E8E ",L"\x5206\x533A\x4FE1\x606F...",L"\x6807\x5934\x4FE1\x606F...",L"\x6247\x533A\x67E5\x770B\x5668...",L"\x6267\x884C",L"\x9000\x51FA",L"(\x5982\x679C\x4E0D\x4E86\x89E3 UUID\xFF0C\x8BF7\x4FDD\x6301\x6B64\x9879\x52FE\x9009\xFF01)",L"\x8BED\x8A00:",L"VDI-Tools 6.00\x3002\x57FA\x4E8E CloneVDI\xFF0C\x7248\x6743\x6240\x6709 \x00A9 2010 Don Milne\x3002"},
 {L"VDI-Tools 6.00",L" Dateien ",L"Quelle:",L"Ziel:",L"Durchsuchen...",L" Informationen zum Quelldatentr\x00E4ger ",L"Pr\x00FC" L"fergebnis:",L"Datentr\x00E4gergr\x00F6\x00DF" L"e:",L"Dateisystem:",L"Verschl\x00FCsselung:",L" Optionen ",L"Neue VDI erstellen",L"Quell-VDI direkt \x00E4ndern",L"Neue UUID erzeugen",L"Alte UUID behalten",L"Virtuelle Datentr\x00E4gergr\x00F6\x00DF" L"e erh\x00F6hen auf",L"Partitionsgr\x00F6\x00DF" L"e erh\x00F6hen",L"Nicht verwendete Bl\x00F6cke komprimieren",L" Info ",L"Partitions&info...",L"&Headerinfo...",L"&Sektoransicht...",L"&Ausf\x00FChren",L"&Beenden",L"(Aktiviert lassen, wenn Sie UUID nicht kennen.)",L"Sprache:",L"VDI-Tools 6.00. Basiert auf CloneVDI \x00A9 2010 Don Milne."},
 {L"VDI-Tools 6.00",L"\x30D5\x30A1\x30A4\x30EB",L"\x30BD\x30FC\x30B9:",L"\x51FA\x529B\x5148:",L"\x53C2\x7167...",L"\x30BD\x30FC\x30B9\x30C9\x30E9\x30A4\x30D6\x60C5\x5831",L"\x691C\x8A3C\x7D50\x679C:",L"\x30C9\x30E9\x30A4\x30D6\x5BB9\x91CF:",L"\x30D5\x30A1\x30A4\x30EB\x30B7\x30B9\x30C6\x30E0:",L"\x6697\x53F7\x5316:",L"\x30AA\x30D7\x30B7\x30E7\x30F3",L"\x65B0\x3057\x3044 VDI \x3092\x4F5C\x6210",L"\x30BD\x30FC\x30B9 VDI \x3092\x76F4\x63A5\x5909\x66F4",L"\x65B0\x3057\x3044 UUID \x3092\x751F\x6210",L"\x65E7\x3044 UUID \x3092\x4FDD\x6301",L"\x4EEE\x60F3\x30C9\x30E9\x30A4\x30D6\x5BB9\x91CF\x3092\x6B21\x307E\x62E1\x5F35",L"\x30D1\x30FC\x30C6\x30A3\x30B7\x30E7\x30F3\x30B5\x30A4\x30BA\x3092\x62E1\x5F35",L"\x672A\x4F7F\x7528\x30D6\x30ED\x30C3\x30AF\x3092\x5727\x7E2E",L"\x60C5\x5831",L"\x30D1\x30FC\x30C6\x30A3\x30B7\x30E7\x30F3\x60C5\x5831...",L"\x30D8\x30C3\x30C0\x60C5\x5831...",L"\x30BB\x30AF\x30BF\x8868\x793A...",L"\x5B9F\x884C",L"\x7D42\x4E86",L"(UUID \x304C\x4E0D\x660E\x306A\x5834\x5408\x306F\x30C1\x30A7\x30C3\x30AF\x3092\x7EF4\x6301\x3057\x3066\x304F\x3060\x3055\x3044\xFF01)",L"\x8A00\x8A9E:",L"VDI-Tools 6.00. CloneVDI \x00A9 2010 Don Milne \x3092\x57FA\x306B\x3057\x3066\x3044\x307E\x3059\x3002"},
 {L"VDI-Tools 6.00",L" Fichiers ",L"Source :",L"Destination :",L"Parcourir...",L" Informations du disque source ",L"R\x00E9sultat de validation :",L"Taille du disque :",L"Syst\x00E8me de fichiers :",L"Chiffrement :",L" Options ",L"Cr\x00E9er un nouveau VDI",L"Modifier le VDI source sur place",L"G\x00E9n\x00E9rer un nouvel UUID",L"Conserver l'ancien UUID",L"Augmenter la taille du disque virtuel \x00E0",L"Augmenter la taille de la partition",L"Compacter les blocs inutilis\x00E9s",L" \x00C0 propos ",L"&Informations de partition...",L"Informations d'&en-t\x00EAte...",L"Visionneuse de &secteurs...",L"&Ex\x00E9cuter",L"&Quitter",L"(Gardez cette option coch\x00E9" L"e si vous ne connaissez pas les UUID.)",L"Langue :",L"VDI-Tools 6.00. Bas\x00E9 sur CloneVDI \x00A9 2010 Don Milne."},
 {L"VDI-Tools 6.00",L" Archivos ",L"Origen:",L"Destino:",L"Examinar...",L" Informaci\x00F3n de la unidad de origen ",L"Resultado de validaci\x00F3n:",L"Tama\x00F1o de la unidad:",L"Sistema de archivos:",L"Cifrado:",L" Opciones ",L"Crear un VDI nuevo",L"Modificar el VDI de origen directamente",L"Generar UUID nuevo",L"Conservar UUID anterior",L"Aumentar tama\x00F1o de unidad virtual a",L"Aumentar tama\x00F1o de partici\x00F3n",L"Compactar bloques sin usar",L" Acerca de ",L"Informaci\x00F3n de &partici\x00F3n...",L"Informaci\x00F3n de &cabecera...",L"Visor de &sectores...",L"&Continuar",L"&Salir",L"(Mantenga esta opci\x00F3n marcada si no sabe qu\x00E9 es un UUID.)",L"Idioma:",L"VDI-Tools 6.00. Basado en CloneVDI \x00A9 2010 Don Milne."},
 {L"VDI-Tools 6.00",L"\x6A94\x6848",L"\x4F86\x6E90:",L"\x76EE\x6A19:",L"\x700F\x89BD...",L"\x4F86\x6E90\x78C1\x789F\x8CC7\x8A0A",L"\x9A57\x8B49\x7D50\x679C:",L"\x78C1\x789F\x5BB9\x91CF:",L"\x6A94\x6848\x7CFB\x7D71:",L"\x52A0\x5BC6:",L"\x9078\x9805",L"\x5EFA\x7ACB\x65B0\x7684 VDI",L"\x76F4\x63A5\x4FEE\x6539\x4F86\x6E90 VDI",L"\x7522\x751F\x65B0 UUID",L"\x4FDD\x7559\x820A UUID",L"\x5C07\x865B\x64EC\x78C1\x789F\x5BB9\x91CF\x589E\x52A0\x81F3",L"\x589E\x52A0\x78C1\x789F\x5206\x5272\x5340\x5927\x5C0F",L"\x58D3\x7E2E\x672A\x4F7F\x7528\x5340\x584A",L"\x95DC\x65BC",L"\x5206\x5272\x5340\x8CC7\x8A0A...",L"\x6A19\x982D\x8CC7\x8A0A...",L"\x78C1\x5340\x6AA2\x8996\x5668...",L"\x57F7\x884C",L"\x7D50\x675F",L"(\x5982\x4E0D\x77E5 UUID \x70BA\x4F55\x7269\xFF0C\x8ACB\x4FDD\x6301\x52FE\x9078\x3002)",L"\x8A9E\x8A00:",L"VDI-Tools 6.00\x3002\x57FA\x65BC CloneVDI \x00A9 2010 Don Milne\x3002"},
 {L"VDI-Tools 6.00",L" \xD30C\xC77C ",L"\xC6D0\xBCF8:",L"\xB300\xC0C1:",L"\xCC3E\xC544...",L" \xC6D0\xBCF8 \xB514\xC2A4\xBE0C \xC815\xBCF4 ",L"\xAC80\xC99D \xACB0\xACFC:",L"\xB514\xC2A4\xBE0C \xD06C\xAE30:",L"\xD30C\xC77C \xC2DC\xC2A4\xD15C:",L"\xC554\xD638\xD654:",L" \xC635\xC158 ",L"\xC0C8 VDI \xB9CC\xB4E4\xAE30",L"\xC6D0\xBCF8 VDI\xB97C \xC9C1\xC811 \xC218\xC815",L"\xC0C8 UUID \xC0DD\xC131",L"\xAE30\xC874 UUID \xC720\xC9C0",L"\xAC00\xC0C1 \xB514\xC2A4\xBE0C \xD06C\xAE30\xB97C \xB2E4\xC74C\xC73C\xB85C",L"\xD30C\xD2F0\xC158 \xD06C\xAE30 \xB2E4\xC74C\xB85C",L"\xC0AC\xC6A9\xD558\xC9C0 \xC54A\xB294 \xBE14\xB85D \xC555\xCD95",L" \xC815\xBCF4 ",L"\xD30C\xD2F0\xC158 \xC815\xBCF4...",L"\xD5E4\xB354 \xC815\xBCF4...",L"\xC139\xD130 \xBDF0\xC5B4...",L"\xC2E4\xD589",L"\xB05D\xB8CC",L"(UUID\xB97C \xBB34\xC5C7\xC778\xBA74 \xC774 \xD56D\xBAA9\xC744 \xCCB4\xD06C\xD558\xC138\xC694.)",L"\xC5B8\xC5B4:",L"VDI-Tools 6.00. CloneVDI \x00A9 2010 Don Milne \xAE30\xBC18."},
 {L"VDI-Tools 6.00",L" \x0424\x0430\x0439\x043B\x044B ",L"\x0418\x0441\x0442\x043E\x0447\x043D\x0438\x043A:",L"\x041D\x0430\x0437\x043D\x0430\x0447\x0435\x043D\x0438\x0435:",L"\x041E\x0431\x0437\x043E\x0440...",L" \x0418\x043D\x0444\x043E\x0440\x043C\x0430\x0446\x0438\x044F \x043E\x0431 \x0438\x0441\x0445\x043E\x0434\x043D\x043E\x043C \x0434\x0438\x0441\x043A\x0435 ",L"\x0420\x0435\x0437\x0443\x043B\x044C\x0442\x0430\x0442 \x043F\x0440\x043E\x0432\x0435\x0440\x043A\x0438:",L"\x0420\x0430\x0437\x043C\x0435\x0440 \x0434\x0438\x0441\x043A\x0430:",L"\x0424\x0430\x0439\x043B\x043E\x0432\x0430\x044F \x0441\x0438\x0441\x0442\x0435\x043C\x0430:",L"\x0428\x0438\x0444\x0440\x043E\x0432\x0430\x043D\x0438\x0435:",L" \x041F\x0430\x0440\x0430\x043C\x0435\x0442\x0440\x044B ",L"\x0421\x043E\x0437\x0434\x0430\x0442\x044C \x043D\x043E\x0432\x044B\x0439 VDI",L"\x0418\x0437\x043C\x0435\x043D\x0438\x0442\x044C \x0438\x0441\x0445\x043E\x0434\x043D\x044B\x0439 VDI",L"\x0421\x043E\x0437\x0434\x0430\x0442\x044C \x043D\x043E\x0432\x044B\x0439 UUID",L"\x0421\x043E\x0445\x0440\x0430\x043D\x0438\x0442\x044C UUID",L"\x0423\x0432\x0435\x043B\x0438\x0447\x0438\x0442\x044C \x0440\x0430\x0437\x043C\x0435\x0440 \x0434\x0438\x0441\x043A\x0430 \x0434\x043E",L"\x0423\x0432\x0435\x043B\x0438\x0447\x0438\x0442\x044C \x0440\x0430\x0437\x043C\x0435\x0440 \x0440\x0430\x0437\x0434\x0435\x043B\x0430",L"\x0421\x0436\x0430\x0442\x044C \x043D\x0435\x0438\x0441\x043F\x043E\x043B\x044C\x0437\x0443\x0435\x043C\x044B\x0435 \x0431\x043B\x043E\x043A\x0438",L" \x041E \x043F\x0440\x043E\x0433\x0440\x0430\x043C\x043C\x0435 ",L"\x0418\x043D\x0444\x043E \x043E \x0440\x0430\x0437\x0434\x0435\x043B\x0430\x0445...",L"\x0418\x043D\x0444\x043E \x043E \x0437\x0430\x0433\x043E\x043B\x043E\x0432\x043A\x0435...",L"\x041F\x0440\x043E\x0441\x043C\x043E\x0442\x0440 \x0441\x0435\x043A\x0442\x043E\x0440\x043E\x0432...",L"\x0412\x044B\x043F\x043E\x043B\x043D\x0438\x0442\x044C",L"\x0412\x044B\x0439\x0442\x0438",L"(\x041E\x0441\x0442\x0430\x0432\x044C\x0442\x0435 \x0432\x043A\x043B\x044E\x0447\x0435\x043D\x043D\x044B\x043C, \x0435\x0441\x043B\x0438 \x043D\x0435 \x0437\x043D\x0430\x0435\x0442\x0435, \x0447\x0442\x043E \x0442\x0430\x043A\x043E\x0435 UUID.)",L"\x042F\x0437\x044B\x043A:",L"VDI-Tools 6.00. \x041D\x0430 \x043E\x0441\x043D\x043E\x0432\x0435 CloneVDI \x00A9 2010 Don Milne."},
 {L"VDI-Tools 6.00",L" Arquivos ",L"Origem:",L"Destino:",L"Procurar...",L" Informa\x00E7\x00F5es da unidade de origem ",L"Resultado da valida\x00E7\x00E3o:",L"Tamanho da unidade:",L"Sistema de arquivos:",L"Criptografia:",L" Op\x00E7\x00F5es ",L"Criar um novo VDI",L"Modificar VDI de origem no local",L"Gerar novo UUID",L"Manter UUID antigo",L"Aumentar o tamanho da unidade virtual para",L"Aumentar tamanho da parti\x00E7\x00E3o",L"Compactar blocos n\x00E3o usados",L" Sobre ",L"Informa\x00E7\x00F5es da &parti\x00E7\x00E3o...",L"Informa\x00E7\x00F5es do &cabe\x00E7alho...",L"Visualizador de &setores...",L"&Prosseguir",L"&Sair",L"(Mantenha esta op\x00E7\x00E3o marcada se n\x00E3o souber o que \x00E9 UUID.)",L"Idioma:",L"VDI-Tools 6.00. Baseado em CloneVDI \x00A9 2010 Don Milne."}
};

int Localization_ResolveLanguage(int preference)
{
   LANGID id;
   if (preference >= VDI_LANGUAGE_EN && preference <= VDI_LANGUAGE_PT_BR) return preference;
   id = GetUserDefaultUILanguage();
   switch (PRIMARYLANGID(id)) {
      case LANG_CHINESE: return SUBLANGID(id)==SUBLANG_CHINESE_SIMPLIFIED || SUBLANGID(id)==SUBLANG_CHINESE_SINGAPORE ? VDI_LANGUAGE_ZH_CN : VDI_LANGUAGE_ZH_TW;
      case LANG_GERMAN: return VDI_LANGUAGE_DE;
      case LANG_JAPANESE: return VDI_LANGUAGE_JA;
      case LANG_FRENCH: return VDI_LANGUAGE_FR;
      case LANG_SPANISH: return VDI_LANGUAGE_ES;
      case LANG_KOREAN: return VDI_LANGUAGE_KO;
      case LANG_RUSSIAN: return VDI_LANGUAGE_RU;
      case LANG_PORTUGUESE: return VDI_LANGUAGE_PT_BR;
   }
   return VDI_LANGUAGE_EN;
}

void Localization_PopulateLanguageCombo(HWND hCombo, int preference)
{
   int i, selection = preference == VDI_LANGUAGE_AUTO ? 0 : preference + 1;
   SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
   for (i=0; i<(int)(sizeof(languageNames)/sizeof(languageNames[0])); i++) {
      LRESULT index = SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)languageNames[i]);
      SendMessageW(hCombo, CB_SETITEMDATA, index, (LPARAM)(i-1));
   }
   SendMessageW(hCombo, CB_SETCURSEL, selection, 0);
}

int Localization_GetLanguageComboValue(HWND hCombo)
{
   LRESULT index = SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
   return index == CB_ERR ? VDI_LANGUAGE_AUTO : (int)SendMessageW(hCombo, CB_GETITEMDATA, index, 0);
}

void Localization_ApplyMainDialog(HWND hDlg, int preference)
{
   const UI_TEXT *t = &uiText[Localization_ResolveLanguage(preference)];
   SetWindowTextW(hDlg,t->title);
   SetDlgItemTextW(hDlg,IDD_GROUP_FILES,t->files); SetDlgItemTextW(hDlg,IDD_LABEL_SOURCE,t->source);
   SetDlgItemTextW(hDlg,IDD_LABEL_DESTINATION,t->destination); SetDlgItemTextW(hDlg,201,t->browse); SetDlgItemTextW(hDlg,203,t->browse);
   SetDlgItemTextW(hDlg,IDD_GROUP_SOURCEINFO,t->sourceInfo); SetDlgItemTextW(hDlg,IDD_LABEL_VALIDATION,t->validation);
   SetDlgItemTextW(hDlg,IDD_LABEL_DRIVESIZE,t->driveSize); SetDlgItemTextW(hDlg,IDD_LABEL_FILESYSTEM,t->fileSystem); SetDlgItemTextW(hDlg,IDD_LABEL_ENCRYPTION,t->encryption);
   SetDlgItemTextW(hDlg,IDD_GROUP_OPTIONS,t->options); SetDlgItemTextW(hDlg,214,t->newVdi); SetDlgItemTextW(hDlg,215,t->inPlace);
   SetDlgItemTextW(hDlg,207,t->newUuid); SetDlgItemTextW(hDlg,208,t->keepUuid); SetDlgItemTextW(hDlg,209,t->increaseSize);
   SetDlgItemTextW(hDlg,211,t->increasePartition); SetDlgItemTextW(hDlg,212,t->compact); SetDlgItemTextW(hDlg,IDD_GROUP_ABOUT,t->about);
   SetDlgItemTextW(hDlg,300,t->partitionInfo); SetDlgItemTextW(hDlg,301,t->headerInfo); SetDlgItemTextW(hDlg,302,t->sectorViewer);
   SetDlgItemTextW(hDlg,1,t->proceed); SetDlgItemTextW(hDlg,2,t->exit); SetDlgItemTextW(hDlg,IDD_UUID_HINT,t->uuidHint);
   SetDlgItemTextW(hDlg,IDD_LABEL_LANGUAGE,t->language); SetDlgItemTextW(hDlg,213,t->aboutText);
}
