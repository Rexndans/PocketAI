// =====================================================================
//  POCKET AI CASE (parametrik) - untuk 3D print
//  Buka di OpenSCAD (gratis), ubah angka di bawah sesuai komponenmu,
//  tekan F6 (Render), lalu File > Export > Export as STL.
//
//  Susunan dari depan ke belakang:
//    [pelat depan: jendela OLED + 5 tombol]
//    [lapisan OLED]
//    [lapisan ESP32-S3]
//    [lapisan belakang: charger (kiri), baterai (kanan),
//     boost MT3608 ditumpuk di atas baterai]
//    [tutup belakang, tekan masuk]
//
//  Cetak: body dengan sisi depan di bawah, lid dengan pelat di bawah.
//  Layar menghadap depan di sisi z = 0.
// =====================================================================

part = "both";   // "body", "lid", atau "both" (both = untuk pratinjau)

// ---------- UKURAN KOMPONEN (mm) - UKUR PUNYAMU ----------
esp_l  = 69;     // panjang board ESP32-S3
esp_w  = 26;     // lebar board
esp_h  = 8;      // tebal board + komponen (tanpa pin header = sekitar 6-8)

oled_l = 29;     // modul OLED, sisi sejajar panjang casing
oled_w = 27.4;   // modul OLED, sisi sejajar lebar casing (pin header di sisi atas)
oled_h = 5;      // tebal OLED + solder kabel

bat_l  = 40;     // baterai LiPo (kode 603040 = tebal 6, lebar 30, panjang 40)
bat_w  = 30;
bat_t  = 6;

chg_l  = 28;     // modul charger (TP4056 sekitar 26 x 17)
chg_w  = 17;
chg_h  = 5;

boost_l = 37;    // modul boost MT3608 (sekitar 37 x 17), ditempel di atas baterai
boost_w = 17;
boost_h = 6;     // tebal modul + induktor

// ---------- JENDELA LAYAR ----------
win_w        = 24;   // lebar jendela
win_h        = 13;   // tinggi jendela
win_from_top = 5;    // jarak tepi atas PCB OLED ke tepi atas jendela

// ---------- TOMBOL ----------
btn_d     = 7;       // diameter lubang tombol
btn_pitch = 9;       // jarak antar pusat tombol (pola plus)

// ---------- LUBANG SAMPING ----------
usb_esp_w = 24;      // lebar lubang USB ESP32 (dua port)
usb_h     = 7;       // tinggi lubang USB ESP32
usb_chg_w = 11;      // lebar lubang USB charger
sw_w      = 9;       // lebar lubang saklar
sw_h      = 4;       // tinggi lubang saklar

// ---------- CANGKANG ----------
wall    = 1.6;       // tebal dinding
t_front = 1.6;       // tebal pelat depan
t_lid   = 1.6;       // tebal tutup
gap     = 0.8;       // jarak longgar sisi lebar
end_gap = 1.4;       // jarak longgar sisi panjang (tempat lip tutup)
tol     = 0.2;       // toleransi tutup
lip_t   = 1.0;       // tebal lip tutup
lip_d   = 2.5;       // kedalaman lip tutup
r       = 2;         // radius sudut luar

// ---------- HITUNGAN OTOMATIS ----------
IL = max(esp_l, bat_l + chg_l + 1) + 2 * end_gap;   // panjang dalam
IW = max(esp_w, max(bat_w, oled_w)) + 2 * gap;      // lebar dalam
layer_back = max(bat_t + boost_h, chg_h);
assert(boost_l <= bat_l && boost_w <= bat_w, "Boost tidak muat di atas baterai, perbesar casing");
IH = oled_h + esp_h + layer_back + 0.8;             // tinggi dalam
OL = IL + 2 * wall;
OW = IW + 2 * wall;
OH = t_front + IH;

z_esp0  = t_front + oled_h;
z_back0 = z_esp0 + esp_h;

oled_x0   = wall + end_gap;               // sisi kiri PCB OLED
pcb_top_y = OW / 2 + oled_w / 2;          // sisi atas PCB OLED (pin header di sini)
zone_x0   = oled_x0 + oled_l + 3;
zone_x1   = wall + IL - end_gap;
bx        = (zone_x0 + zone_x1) / 2;      // pusat pola tombol
by        = OW / 2;

echo("Ukuran luar (panjang x lebar x tebal) mm:", OL, OW, OH + t_lid);

module rbox(l, w, h, rad) {
  hull() for (x = [rad, l - rad], y = [rad, w - rad])
    translate([x, y, 0]) cylinder(r = rad, h = h, $fn = 24);
}

module body() {
  difference() {
    rbox(OL, OW, OH, r);

    // rongga (terbuka ke belakang)
    translate([wall, wall, t_front])
      rbox(IL, IW, IH + 1, 0.4);

    // jendela OLED
    translate([oled_x0 + oled_l / 2 - win_w / 2,
               pcb_top_y - win_from_top - win_h,
               -0.1])
      cube([win_w, win_h, t_front + 0.2]);

    // 5 tombol (pola plus): tengah=OK, kiri=MUNDUR, kanan=MAJU, atas=HAPUS, bawah=KIRIM
    for (p = [[0, 0], [-1, 0], [1, 0], [0, 1], [0, -1]])
      translate([bx + p[0] * btn_pitch, by + p[1] * btn_pitch, -0.1])
        cylinder(d = btn_d, h = t_front + 0.2, $fn = 32);

    // lubang USB ESP32 (dinding kanan)
    translate([OL - wall - 0.1, OW / 2 - usb_esp_w / 2, z_esp0 + (esp_h - usb_h) / 2])
      cube([wall + 0.2, usb_esp_w, usb_h]);

    // lubang USB charger (dinding kiri, charger di kiri supaya
    // ujung antena ESP32 jauh dari baterai)
    translate([-0.1, OW / 2 - usb_chg_w / 2, z_back0 + 0.2])
      cube([wall + 0.2, usb_chg_w, min(layer_back, 5)]);

    // lubang saklar (dinding samping bawah)
    translate([OL / 2 - sw_w / 2, -0.1, z_back0 + (layer_back - sw_h) / 2 + 0.2])
      cube([sw_w, wall + 0.2, sw_h]);
  }
}

module lid() {
  rbox(OL, OW, t_lid, r);
  for (x = [wall + tol, wall + IL - tol - lip_t])
    translate([x, wall + tol, t_lid])
      cube([lip_t, IW - 2 * tol, lip_d]);
}

if (part == "body") body();
else if (part == "lid") lid();
else {
  body();
  translate([0, OW + 10, 0]) lid();
}
