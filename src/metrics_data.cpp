#include "quest/metrics_data.hpp"
#include "quest/core.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace quest {
namespace {
double numeric(const std::string& text){
    auto value=trim(text);if(value.empty())return std::numeric_limits<double>::quiet_NaN();
    std::istringstream in(value);in.imbue(std::locale::classic());double result;in>>result;
    if(in.fail()||!in.eof()||!std::isfinite(result))return std::numeric_limits<double>::quiet_NaN();return result;
}
std::string timeKey(std::string s){s=lower(trim(s));s.erase(std::remove_if(s.begin(),s.end(),[](char c){return c==' '||c=='_'||c=='-';}),s.end());return s;}
}
MetricsCsv parseMetricsCsv(const std::string& bytes,bool growing,const std::atomic<bool>* cancelled){
    if(bytes.size()>32*1024*1024)throw std::runtime_error("CSV limité à 32 Mo par session.");
    if(bytes.find('\0')!=std::string::npos)throw std::runtime_error("Le CSV contient des octets nuls (UTF-8 requis).");
    MetricsCsv data;data.raw=bytes;std::vector<std::string> row;std::string cell;bool quoted=false,closed=false,atStart=true,header=false;size_t timeColumn=0,line=1;size_t rowStart=0;
    auto finish=[&]{
        row.push_back(std::move(cell));cell.clear();closed=false;atStart=true;
        if(row.size()==1&&trim(row[0]).empty()){row.clear();return;}
        if(!header){
            if(row.size()<2||row.size()>257)throw std::runtime_error("CSV : entre 2 et 257 colonnes sont requises.");
            if(row[0].rfind("\xef\xbb\xbf",0)==0)row[0].erase(0,3);
            std::set<std::string> names;bool found=false;
            for(size_t i=0;i<row.size();++i){row[i]=trim(row[i]);if(row[i].empty()||row[i].size()>512||!names.insert(lower(row[i])).second)throw std::runtime_error("En-tête CSV vide, dupliqué ou trop long.");auto key=timeKey(row[i]);if(key=="timestamp"||key=="timestamp(ms)"){if(found)throw std::runtime_error("Plusieurs colonnes de temps.");timeColumn=i;found=true;}}
            if(!found)throw std::runtime_error("Colonne OVR Time Stamp / timestamp en millisecondes absente.");
            for(size_t i=0;i<row.size();++i)if(i!=timeColumn)data.columns.push_back(row[i]);header=true;
        }else{
            if(row.size()!=data.columns.size()+1)throw std::runtime_error("Nombre de colonnes incohérent à la ligne "+std::to_string(line)+".");
            auto stamp=numeric(row[timeColumn]);if(!std::isfinite(stamp))throw std::runtime_error("Timestamp invalide à la ligne "+std::to_string(line)+".");
            if(data.times.empty())data.firstTimestamp=stamp;auto time=(stamp-data.firstTimestamp)/1000.;
            if(!std::isfinite(time))throw std::runtime_error("Plage de timestamps invalide.");
            if(!data.times.empty()&&time<data.times.back())throw std::runtime_error("Le temps recule dans ce CSV : sessions distinctes requises.");
            if(data.times.size()>=200000||data.values.size()+data.columns.size()>4000000)throw std::runtime_error("Session trop grande : limite de 200 000 lignes / 4 millions de valeurs.");
            data.times.push_back(time);for(size_t i=0;i<row.size();++i)if(i!=timeColumn)data.values.push_back(numeric(row[i]));
        }row.clear();
    };
    for(size_t i=0;i<bytes.size();++i){
        if((i&4095)==0&&cancelled&&*cancelled)throw std::runtime_error("Lecture CSV annulée.");char c=bytes[i];
        if(quoted){if(c=='"'){if(i+1<bytes.size()&&bytes[i+1]=='"'){cell+='"';++i;}else{quoted=false;closed=true;}}else cell+=c;}
        else if(c=='"'){if(!atStart)throw std::runtime_error("Guillemet au milieu d’une cellule CSV non protégée.");quoted=true;atStart=false;}
        else if(c==','){row.push_back(std::move(cell));cell.clear();closed=false;atStart=true;}
        else if(c=='\n'||c=='\r'){
            finish();if(c=='\r'&&i+1<bytes.size()&&bytes[i+1]=='\n')++i;rowStart=i+1;++line;
        }else{if(closed&&c!=' '&&c!='\t')throw std::runtime_error("Texte après une cellule CSV entre guillemets.");if(!closed){cell+=c;atStart=false;}}
        if(cell.size()>256*1024)throw std::runtime_error("Cellule CSV trop longue.");
    }
    if(rowStart<bytes.size()){
        if(growing){data.incompleteRows=1;data.raw.resize(rowStart);}
        else{if(quoted)throw std::runtime_error("Cellule CSV incomplète.");finish();}
    }
    if(!header)throw std::runtime_error("En-tête CSV absent.");return data;
}
MetricsCsv readMetricsCsv(const std::filesystem::path& path,bool growing,const std::atomic<bool>* cancelled){
    std::error_code ec;auto size=std::filesystem::file_size(path,ec);if(ec||size>32*1024*1024)throw std::runtime_error("CSV absent ou supérieur à 32 Mo.");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Ouverture du CSV impossible.");std::string bytes((std::istreambuf_iterator<char>(input)),{});auto result=parseMetricsCsv(bytes,growing,cancelled);result.name=path.filename().u8string();return result;
}
std::string metricsUnit(const std::string& column){auto name=lower(column);if(name.find("microseconds")!=std::string::npos)return "µs";if(name.find("milliseconds")!=std::string::npos)return "ms";if(name.find("_mhz")!=std::string::npos)return "MHz";if(name.find("_mb")!=std::string::npos)return "Mo";if(name.find("percentage")!=std::string::npos||name.find("percent_")==0)return "%";if(name.find("celcius")!=std::string::npos||name.find("celsius")!=std::string::npos)return "°C";if(name=="average_frame_rate"||name.find("frames_per_second")!=std::string::npos)return "FPS";return "";}
MetricsPlot metricsPlot(const MetricsCsv& data,size_t column,double start,double end,size_t pixels){
    MetricsPlot out;if(column>=data.columns.size()||!std::isfinite(start)||!std::isfinite(end)||end<=start||pixels<1||pixels>8192)return out;out.buckets.resize(pixels);double sum=0;
    auto begin=std::lower_bound(data.times.begin(),data.times.end(),start),finish=std::upper_bound(data.times.begin(),data.times.end(),end);
    for(auto it=begin;it!=finish;++it){auto row=size_t(it-data.times.begin());auto x=std::min(pixels-1,size_t((*it-start)/(end-start)*double(pixels)));auto& bucket=out.buckets[x];auto value=data.value(row,column);if(!std::isfinite(value)){bucket.missing=true;continue;}
        if(!bucket.count){bucket.low=bucket.high=bucket.first=value;bucket.firstRow=row;}bucket.low=std::min(bucket.low,value);bucket.high=std::max(bucket.high,value);bucket.last=value;bucket.lastRow=row;++bucket.count;
        if(!out.count)out.low=out.high=value;out.low=std::min(out.low,value);out.high=std::max(out.high,value);++out.count;auto weight=1./double(out.count);sum=sum*(1.-weight)+value*weight;
    }if(out.count)out.average=sum;return out;
}
std::string demoMetricsCsv(){
    std::ostringstream csv;csv.imbue(std::locale::classic());csv<<"Time Stamp,average_frame_rate,app_gpu_time_microseconds,cpu_utilization_percentage,gpu_utilization_percentage,available_memory_MB,battery_temperature_celcius,custom_metric\r\n";
    for(int i=0;i<360;++i){double wave=std::sin(i*.045),spike=i>170&&i<182?15.:0.;csv<<100000+i*100<<','<<std::fixed<<std::setprecision(3)<<90-spike-2*wave<<','<<5400+900*wave+spike*180<<','<<37+8*std::sin(i*.07)<<','<<68+7*wave+spike<<','<<4200-i*.5<<','<<34+i*.006<<',';if(i<130||i>145)csv<<-3+2*wave;csv<<"\r\n";}return csv.str();
}
}
