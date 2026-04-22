// g++ -O2 -o amtxgui_fltk amtxgui_fltk.cpp `fltk-config --use-images --use-forms --cxxflags --ldflags`
// static build: 
// sudo apt-get update
// sudo apt-get install libx11-dev libxext-dev libxinerama-dev libxft-dev \
//                     libxcursor-dev libxrender-dev libxfixes-dev \
//                     libpng-dev libjpeg-dev libfontconfig1-dev zlib1g-dev
// g++ -O2 -o amtxgui_fltk amtxgui_fltk.cpp $(fltk-config --use-images --use-forms --cxxflags --ldstaticflags)
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

struct Station { std::string freq, bw, name, url; };

struct LangSet {
    std::string win_title, col_freq, col_bw, col_name, col_url, btn_start, btn_stop, status_label, 
                 menu_file, menu_load, menu_save, menu_sender, menu_add, menu_del, menu_lang,
                 dlg_title, dlg_plan, dlg_freq, dlg_bw, dlg_prog, dlg_url, dlg_btn, err_freq, err_title;
};

void trim_r(std::string &s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
}

std::string clean_str(std::string v) {
    std::regex re("(\\d+(\\.\\d+)?)"); std::smatch m;
    return std::regex_search(v, m, re) ? m.str(1) : "0";
}

std::map<std::string, LangSet> LANGUAGES = {
    {"DE", {"AM Transmission Multi-Stream Control", "Frequenz", "Bandbreite", "Programmname", "URL", "ALLE Sender STARTEN", "ALLE Sender STOPPEN", "Modulator Status (fl2k_tcp or ext.SDR):", "Datei", "Laden", "Speichern", "Sender", "Hinzufügen", "Entfernen", "Sprache", "Sender-Parameter", "Frequenzplan:", "Frequenz:", "Audiobandbreite:", "Programm wählen:", "Programm-URL:", "Hinzufügen", "Frequenz belegt!", "Fehler"}},
    {"EN", {"AM Transmission Multi-Stream Control", "Frequency", "Bandwidth", "Program Name", "URL", "START ALL STATIONS", "STOP ALL STATIONS", "Modulator Status (fl2k_tcp or ext.SDR):", "File", "Load", "Save", "Station", "Add", "Remove", "Language", "Station Parameters", "Frequency Plan:", "Frequency:", "Audio Bandwidth:", "Select Program:", "Program URL:", "Add", "Frequency occupied!", "Error"}},
    {"FR", {"AM Transmission Multi-Stream Control", "Fréquence", "Bande passante", "Nom du programme", "URL", "DÉMARRER TOUS LES ÉMETTEURS", "ARRÊTER TOUS LES ÉMETTEURS", "Statut du modulateur (fl2k_tcp or ext.SDR) :", "Fichier", "Charger", "Enregistrer", "Émetteur", "Ajouter", "Supprimer", "Langue", "Paramètres de l'émetteur", "Plan de fréquences :", "Fréquence :", "Bande passante audio :", "Choisir le programme :", "URL du programme :", "Ajouter", "Fréquence occupée !", "Erreur"}},
    {"IT", {"AM Transmission Multi-Stream Control", "Frequenza", "Larghezza di banda", "Nome programma", "URL", "AVVIA TUTTE LE STAZIONI", "FERMA TUTTE LE STAZIONI", "Stato modulatore (fl2k_tcp or ext.SDR):", "File", "Carica", "Salva", "Stazione", "Aggiungi", "Rimuovi", "Lingua", "Parametri della stazione", "Piano di frequenza:", "Frequenza:", "Larghezza di banda audio:", "Seleziona programma:", "URL del programma:", "Aggiungi", "Frequenza occupata!", "Errore"}},
    {"JA", {"AMマルチストリーム送信制御", "周波数", "帯域幅", "番組名", "URL", "全送信機を開始", "全送信機を停止", "変調器ステータス (fl2k_tcp or ext.SDR):", "ファイル", "読み込み", "保存", "送信局", "追加", "削除", "言語 (Language)", "送信パラメータ", "周波数プラン:", "周波数:", "音声帯域幅:", "番組を選択:", "番組URL:", "追加", "周波数が重複しています！", "エラー"}}
};

class StatusLight : public Fl_Box {
public:
    StatusLight(int x, int y, int w, int h) : Fl_Box(x, y, w, h) {}
    void draw() override {
        fl_color(parent()->color()); fl_rectf(x(), y(), w(), h());
        fl_color(color()); fl_pie(x()+5, y()+5, w()-10, h()-10, 0, 360);
        fl_color(FL_BLACK); fl_arc(x()+5, y()+5, w()-10, h()-10, 0, 360);
    }
};

class StationTable : public Fl_Table_Row {
public:
    std::vector<Station> data;
    StationTable(int x, int y, int w, int h) : Fl_Table_Row(x, y, w, h) {
        cols(4); col_header(1); col_width_all(110); col_width(3, 400); col_resize(1); end();
    }
    void draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H) override {
        switch (context) {
            case CONTEXT_COL_HEADER: 
                fl_push_clip(X, Y, W, H); fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND_COLOR);
                fl_color(FL_BLACK); { const char* h[] = {"Freq", "BW", "Name", "URL"}; fl_draw(h[C], X, Y, W, H, FL_ALIGN_CENTER); }
                fl_pop_clip(); break;
            case CONTEXT_CELL:
                fl_push_clip(X, Y, W, H); fl_color(row_selected(R) ? FL_SELECTION_COLOR : FL_WHITE); fl_rectf(X, Y, W, H);
                fl_color(FL_BLACK); { std::string s = (C==0?data[R].freq:C==1?data[R].bw:C==2?data[R].name:data[R].url); fl_draw(s.c_str(), X+5, Y, W, H, FL_ALIGN_LEFT); }
                fl_pop_clip(); break;
            default: break;
        }
    }
};

class SenderDialog : public Fl_Double_Window {
    Fl_Choice *c_plan; Fl_Input_Choice *c_freq, *c_bw, *c_name; Fl_Input *i_url; bool confirmed = false;
    std::map<std::string, std::string> db; std::vector<std::string> exist; const LangSet& L;
public:
    bool is_confirmed() const { return confirmed; } 
    SenderDialog(const LangSet& lang, const std::map<std::string, std::string>& station_db, std::vector<std::string> existing) 
        : Fl_Double_Window(480, 250, lang.dlg_title.c_str()), db(station_db), exist(existing), L(lang) {
        set_modal();
        c_plan = new Fl_Choice(150, 10, 300, 25, L.dlg_plan.c_str());
        const char* p[]={"Europa (9 kHz Raster)","USA (10 kHz Raster)","CH HF-Telefonrundspruch","IT Filodiffusione (RAI)","Manuelle Eingabe"};
        for(auto x:p) c_plan->add(x); c_plan->callback(update_freq_cb, this);
        c_freq = new Fl_Input_Choice(150, 40, 300, 25, L.dlg_freq.c_str());
        c_bw = new Fl_Input_Choice(150, 70, 300, 25, L.dlg_bw.c_str());
        const char* bws[]={"4.5 kHz","5.0 kHz","6.0 kHz","7.0 kHz","9.0 kHz","10.0 kHz","12.0 kHz"};
        for(auto b:bws) c_bw->add(b); c_bw->value("4.5 kHz");
        c_name = new Fl_Input_Choice(150, 100, 300, 25, L.dlg_prog.c_str());
        for(auto const& [n,u]:db) c_name->add(n.c_str());
        c_name->callback([](Fl_Widget*, void* d){ 
            SenderDialog* o=(SenderDialog*)d; if(o->c_name->value()) o->i_url->value(o->db[o->c_name->value()].c_str());
        }, this);
        i_url = new Fl_Input(150, 130, 300, 25, L.dlg_url.c_str());
        Fl_Button* b = new Fl_Button(150, 180, 180, 40, L.dlg_btn.c_str());
        b->callback(confirm_cb, this); c_plan->value(0); update_freq_cb(NULL, this); end();
    }
    static void update_freq_cb(Fl_Widget*, void* d) {
        SenderDialog* o=(SenderDialog*)d; o->c_freq->clear(); int v=o->c_plan->value();
        if(v==0){ for(int f=153;f<=279;f+=9)o->c_freq->add((std::to_string(f)+" kHz").c_str()); for(int f=531;f<=1602;f+=9)o->c_freq->add((std::to_string(f)+" kHz").c_str()); }
        else if(v==1){ for(int f=530;f<=1700;f+=10)o->c_freq->add((std::to_string(f)+" kHz").c_str()); }
        else if(v==2){ const char* f[]={"175 kHz","208 kHz","241 kHz","274 kHz","307 kHz","340 kHz"}; for(auto x:f)o->c_freq->add(x); }
        else if(v==3){ const char* f[]={"178 kHz","211 kHz","244 kHz","277 kHz","310 kHz","343 kHz"}; for(auto x:f)o->c_freq->add(x); }
        if(v==4) o->c_freq->value(""); else if(o->c_freq->menubutton()->size()>0) o->c_freq->value(o->c_freq->menubutton()->text(0));
    }
    static void confirm_cb(Fl_Widget*, void* d) {
        SenderDialog* o=(SenderDialog*)d; std::string cur_f = o->c_freq->value() ? o->c_freq->value() : "";
        for(auto &e:o->exist) { if(clean_str(e) == clean_str(cur_f)) { fl_alert("%s", o->L.err_freq.c_str()); return; } }
        o->confirmed=true; o->hide();
    }
    Station get_result() { return {c_freq->value()?c_freq->value():"", c_bw->value()?c_bw->value():"", c_name->value()?c_name->value():"", i_url->value()}; }
};

class RadioApp {
public:
    Fl_Double_Window* win; StationTable* table; StatusLight *ampel; Fl_Box *lbl_status;
    Fl_Button *btn_start, *btn_stop; Fl_Menu_Bar* menu;
    std::string current_lang = "DE"; std::map<std::string, std::string> station_db;

    RadioApp() {
        win = new Fl_Double_Window(950, 480);
        menu = new Fl_Menu_Bar(0, 0, 950, 30);
        table = new StationTable(10, 40, 930, 350);
        btn_start = new Fl_Button(10, 410, 230, 50); btn_start->callback(start_all_cb, this); btn_start->color(0x90ee9000);
        btn_stop = new Fl_Button(250, 410, 230, 50); btn_stop->callback(stop_all_cb, this); btn_stop->color(0xffcccb00);
        lbl_status = new Fl_Box(500, 420, 300, 30); lbl_status->align(FL_ALIGN_RIGHT|FL_ALIGN_INSIDE);
        ampel = new StatusLight(810, 420, 30, 30); ampel->color(FL_RED);
        load_stations_db(); update_ui(); win->end(); win->show();
        Fl::add_timeout(1.0, check_status, this);
    }

    void load_stations_db() {
        std::ifstream f("stations.db"); std::string l;
        while(std::getline(f,l)){ trim_r(l); size_t p=l.find(','); if(p!=std::string::npos) station_db[l.substr(0,p)]=l.substr(p+1); }
    }

    void update_ui() {
        LangSet& L = LANGUAGES[current_lang]; win->label(L.win_title.c_str()); 
        btn_start->label(L.btn_start.c_str()); btn_stop->label(L.btn_stop.c_str()); lbl_status->label(L.status_label.c_str());
        menu->clear();
        menu->add((L.menu_file+"/"+L.menu_load).c_str(), 0, load_csv_cb, this);
        menu->add((L.menu_file+"/"+L.menu_save).c_str(), 0, save_csv_cb, this);
        menu->add((L.menu_sender+"/"+L.menu_add).c_str(), 0, add_cb, this);
        menu->add((L.menu_sender+"/"+L.menu_del).c_str(), 0, del_cb, this);
        const char* lc[] = {"DE", "EN", "FR", "IT", "JA"};
        for(int i=0; i<5; i++){
            menu->add(("Language/"+std::string(lc[i])).c_str(), 0, [](Fl_Widget* w, void* d){
                RadioApp* a=(RadioApp*)d; const Fl_Menu_Item* m=((Fl_Menu_Bar*)w)->mvalue();
                if(m && m->label()) { a->current_lang = m->label(); a->update_ui(); }
            }, this);
        } win->redraw();
    }

    static void check_status(void* d) {
        RadioApp* a=(RadioApp*)d; int r = std::system("pidof fl2k_tcp >/dev/null 2>&1 || pidof socat >/dev/null 2>&1");
        int ec = WEXITSTATUS(r);
        if(r==-1) a->ampel->color(fl_rgb_color(128,128,128)); 
        else if(ec==0) a->ampel->color(fl_rgb_color(0,255,0)); 
        else a->ampel->color(FL_RED);
        a->ampel->redraw(); Fl::repeat_timeout(1.0, check_status, d);
    }

    static void add_cb(Fl_Widget*, void* d) {
        RadioApp* a=(RadioApp*)d; std::vector<std::string> ex; for(auto &s:a->table->data) ex.push_back(s.freq);
        SenderDialog dlg(LANGUAGES[a->current_lang], a->station_db, ex);
        dlg.show(); while(dlg.shown()) Fl::wait();
        if(dlg.is_confirmed()){ 
            a->table->data.push_back(dlg.get_result()); 
            std::sort(a->table->data.begin(), a->table->data.end(), [](const Station& a, const Station& b){
                return std::stod(clean_str(a.freq)) < std::stod(clean_str(b.freq));
            });
            a->table->rows(a->table->data.size()); a->table->redraw(); 
        }
    }

    static void del_cb(Fl_Widget*, void* d) {
        RadioApp* a=(RadioApp*)d; for(int i=0;i<a->table->rows();i++) if(a->table->row_selected(i)){ a->table->data.erase(a->table->data.begin()+i); break; }
        a->table->rows(a->table->data.size()); a->table->redraw();
    }

    static void start_all_cb(Fl_Widget*, void* d) {
        RadioApp* a=(RadioApp*)d; std::string args=""; int p=1234;
        for(auto& s:a->table->data) args += clean_str(s.freq)+" "+clean_str(s.bw)+" \""+s.url+"\" "+std::to_string(p++)+" ";
        if(args=="") return;
	std::string cmd = "xterm -T 'AM-Broadcaster' -e /bin/bash -c 'exec ./start_sender.sh " + args + "' &";
	std::system(cmd.c_str());
        std::system("xterm -T 'fl2k_tcp DAC Transfer' -e /bin/bash -c 'sudo ./start_fl2k.sh' &");
    }

    static void stop_all_cb(Fl_Widget*, void* d){ std::system("/bin/bash stop_sender.sh"); }

    static void load_csv_cb(Fl_Widget*, void* d) {
        RadioApp* a=(RadioApp*)d; Fl_Native_File_Chooser fnc; fnc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fnc.filter("CSV Files (*.csv)\t*.csv"); if(fnc.show()==0){
            a->table->data.clear(); std::ifstream f(fnc.filename()); std::string l; std::getline(f,l);
            while(std::getline(f,l)){ trim_r(l); std::stringstream ss(l); std::string fq,bw,nm,ur; std::getline(ss,fq,';'); std::getline(ss,bw,';'); std::getline(ss,nm,';'); std::getline(ss,ur,';');
                if(!fq.empty()) a->table->data.push_back({fq,bw,nm,ur}); 
            } a->table->rows(a->table->data.size()); a->table->redraw();
        }
    }

    static void save_csv_cb(Fl_Widget*, void* d) {
        RadioApp* a=(RadioApp*)d; Fl_Native_File_Chooser fnc; fnc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        fnc.filter("CSV Files\t*.csv"); if(fnc.show()==0){
            std::ofstream f(fnc.filename()); f<<"Freq;BW;Name;URL\n"; 
            for(auto& s:a->table->data) f<<s.freq<<";"<<s.bw<<";"<<s.name<<";"<<s.url<<"\n";
        }
    }
};

int main() { RadioApp app; return Fl::run(); }
