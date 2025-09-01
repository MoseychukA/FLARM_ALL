#include "config.h"
#include <LittleFS.h>

const char* profile_name(Profile p)
{ 
   switch(p)
   { 
   case Profile::Normal: return "Normal"; 
   case Profile::HighEMI: return "High-EMI"; 
   case Profile::Urban: return "Urban";
   default: return "Remote"; 
   } 
}

void load_config(struct Config& cfg) 
{ 
	if (LittleFS.exists("/config.txt")) 
	{ 
		File f = LittleFS.open("/config.txt", "r"); 
		if (f)
		{ 
			while (f.available()) 
			{
				String line = f.readStringUntil(' '); 
				line.trim();
				if (!line.length() || line[0] == '#') continue; 
				int eq = line.indexOf('='); 
				if (eq < 0) continue; 
				String k = line.substring(0, eq), v = line.substring(eq + 1);
				k.trim(); v.trim(); 
				if (k == "profile") 
				{ 
					if (v == "Normal") cfg.profile = Profile::Normal;
					else if (v == "High-EMI") cfg.profile = Profile::HighEMI; 
					else if (v == "Urban") cfg.profile = Profile::Urban;
					else cfg.profile = Profile::Remote;
				} 
				else if (k == "base_thr") cfg.base_thr = v.toFloat(); 
				else if (k == "k_ema") cfg.k_ema = v.toFloat(); 
				else if (k == "dead_time_us") cfg.dead_time_us = v.toInt();
				else if (k == "min_pos_hits") cfg.min_pos_hits = v.toInt();
				else if (k == "penalty_w") cfg.penalty_w = v.toFloat();
				else if (k == "output_raw") cfg.output_raw = (v == "1" || v == "true" || v == "on"); 
				else if (k == "ref_lat") cfg.ref_lat = v.toFloat(); 
				else if (k == "ref_lon") cfg.ref_lon = v.toFloat();
				else if (k == "ema_alpha") cfg.ema_alpha = v.toFloat(); 
				else if (k == "dyn_k") cfg.dyn_k = v.toFloat(); 
				else if (k == "min_pos_hits_per_win") cfg.min_pos_hits_per_win = v.toInt(); 
				else if (k == "pre_strict_tol") cfg.pre_strict_tol = v.toInt(); 
				else if (k == "short_min_samp") cfg.short_min_samp = v.toInt(); 
				else if (k == "long_max_samp") cfg.long_max_samp = v.toInt(); 
				else if (k == "out_format") cfg.out_format = v.toInt(); } f.close(); 
		}
	} 
}

void save_config(const struct Config& cfg) 
{
	File f = LittleFS.open("/config.txt", "w"); if (!f) return; f.printf("profile=%s", profile_name(cfg.profile));
	f.printf("base_thr=%f", cfg.base_thr); f.printf("k_ema=%f", cfg.k_ema); f.printf("dead_time_us=%d", cfg.dead_time_us); f.printf("min_pos_hits=%d", cfg.min_pos_hits);
	f.printf("penalty_w=%f", cfg.penalty_w); f.printf("output_raw=%d", cfg.output_raw ? 1 : 0); f.printf("ref_lat=%fref_lon=%f", cfg.ref_lat, cfg.ref_lon);
	f.printf("ema_alpha=%f", cfg.ema_alpha); f.printf("dyn_k=%f", cfg.dyn_k); f.printf("min_pos_hits_per_win=%d", cfg.min_pos_hits_per_win);
	f.printf("pre_strict_tol=%d", cfg.pre_strict_tol); f.printf("short_min_samp=%dlong_max_samp=%d", cfg.short_min_samp, cfg.long_max_samp);
	f.printf("out_format=%d", cfg.out_format); f.close();
}
void print_config(const struct Config& cfg) 
{
	Serial.printf(" Profile=%s\r\n nbase_thr=%.1f\r\n nk_ema=%.2f\r\n dead=%dus\r\n minHits=%d\r\n pen=%.2f\r\n RAW=%d\r\n ref=(%.5f,%.5f)\r\n outfmt=%d\r\n ema=%.2f\r\n dynk=%.2f\r\n preTol=%d\r\n short=%d\r\n long=%d\r\n qdrop=%lu\r\n msg=%lu\r\n fix=%lu\r\n fail=%lu\r\n", profile_name(cfg.profile), cfg.base_thr, cfg.k_ema, cfg.dead_time_us, cfg.min_pos_hits, cfg.penalty_w, cfg.output_raw ? 1 : 0, cfg.ref_lat, cfg.ref_lon, cfg.out_format, cfg.ema_alpha, cfg.dyn_k, cfg.pre_strict_tol, cfg.short_min_samp, cfg.long_max_samp, cfg.stat_qdrop, cfg.stat_msgs, cfg.stat_crc_fix, cfg.stat_crc_fail); 
}
