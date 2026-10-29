#include "quest/files.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>

namespace quest {
std::string remotePath(const std::string& path) {
    if(path.empty()||path[0]!='/'||path.size()>4096||path.find('\0')!=std::string::npos)throw std::runtime_error("Chemin Android absolu requis (4096 octets maximum).");
    std::vector<std::string> parts;
    for(size_t at=1;at<=path.size();){auto end=path.find('/',at);if(end==std::string::npos)end=path.size();auto part=path.substr(at,end-at);if(part==".."){if(!parts.empty())parts.pop_back();}else if(!part.empty()&&part!=".")parts.push_back(part);at=end+1;}
    std::string result;for(const auto& part:parts)result+="/"+part;return result.empty()?"/":result;
}
std::string remoteParent(const std::string& path){auto p=remotePath(path);auto at=p.rfind('/');return at==0?"/":p.substr(0,at);}
bool validRemoteName(const std::string& name){return !name.empty()&&name.size()<=255&&name!="."&&name!=".."&&name.find('/')==std::string::npos&&std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32&&c!=127;});}
std::string remoteChild(const std::string& parent,const std::string& name){if(!validRemoteName(name))throw std::runtime_error("Nom invalide : 1–255 octets, sans séparateur ni caractère de contrôle.");return remotePath(parent+(parent=="/"?"":"/")+name);}
static std::string field(const std::string& data,size_t& at){auto end=data.find('\0',at);if(end==std::string::npos)throw std::runtime_error("Liste de fichiers incomplète.");auto result=data.substr(at,end-at);at=end+1;return result;}
template<typename T>static std::optional<T> number(const std::string& text){if(text.empty())return {};if(!std::all_of(text.begin(),text.end(),[](unsigned char c){return c>='0'&&c<='9';}))throw std::runtime_error("Métadonnées de fichier invalides.");size_t used=0;auto n=std::stoull(text,&used);if(n>uint64_t(std::numeric_limits<T>::max()))throw std::runtime_error("Métadonnées de fichier hors limites.");return T(n);}
RemoteListing parseFileListing(const std::string& data) {
    if(data.size()>4*1024*1024)throw std::runtime_error("Liste trop volumineuse.");size_t at=0;RemoteListing result;
    auto header=field(data,at);if(header=="CYVRERROR1")throw std::runtime_error(field(data,at));if(header!="CYVRFILES1")throw std::runtime_error("Réponse de fichiers non reconnue.");
    std::set<std::string> names;
    for(;;){auto kind=field(data,at);if(kind=="END"||kind=="TRUNCATED"){result.truncated=kind=="TRUNCATED";if(at!=data.size())throw std::runtime_error("Données inattendues après la liste.");break;}
        if(kind.size()!=1||std::string("dflo").find(kind[0])==std::string::npos||result.entries.size()>=5000)throw std::runtime_error("Entrée de fichier invalide.");
        RemoteFile entry;entry.type=kind[0];entry.size=number<uint64_t>(field(data,at));entry.modified=number<int64_t>(field(data,at));entry.name=field(data,at);entry.target=field(data,at);
        if(entry.name.empty()||entry.name=="."||entry.name==".."||entry.name.find('/')!=std::string::npos||!names.insert(entry.name).second)throw std::runtime_error("Nom de fichier incohérent dans la réponse.");result.entries.push_back(std::move(entry));
    }
    std::sort(result.entries.begin(),result.entries.end(),[](const RemoteFile& a,const RemoteFile& b){if((a.type=='d')!=(b.type=='d'))return a.type=='d';return lower(a.name)<lower(b.name);});return result;
}
std::string fileListingScript(const std::string& path) {
    return "d="+shellQuote(remotePath(path))+"; test -d \"$d\" && test -r \"$d\" && test -x \"$d\" || { printf 'CYVRERROR1\\000Dossier absent ou accès refusé\\000'; exit 1; }; "
        "printf 'CYVRFILES1\\000'; n=0; for p in \"$d\"/* \"$d\"/.[!.]* \"$d\"/..?*; do "
        "test -e \"$p\" || test -L \"$p\" || continue; if test $n -ge 5000; then printf 'TRUNCATED\\000'; exit 0; fi; "
        "t=o; target=''; if test -L \"$p\"; then t=l; target=$(readlink \"$p\"); elif test -d \"$p\"; then t=d; elif test -f \"$p\"; then t=f; fi; "
        "v=$(stat -c '%s %Y' \"$p\" 2>/dev/null); size=${v%% *}; stamp=${v#* }; "
        "printf '%s\\000%s\\000%s\\000%s\\000%s\\000' \"$t\" \"$size\" \"$stamp\" \"${p##*/}\" \"$target\"; n=$((n+1)); done; printf 'END\\000'";
}
// adb exec-out quotes each argument itself, unlike adb shell's joined command.
JobRequest listRemoteFiles(const std::string& path,const std::wstring& output){return {"Lister "+remotePath(path),{{{"exec-out","sh","-c",fileListingScript(path)},output}},60000,true};}
static std::string fileGuard(const std::string& source){return "test -f "+shellQuote(source)+" && test ! -L "+shellQuote(source)+" || { echo 'Un fichier régulier est requis (liens exclus)' >&2; exit 1; }";}
JobRequest pullRemoteFile(const std::string& source,const std::wstring& destination){auto p=remotePath(source);return {"Récupérer "+p,{{{"shell",fileGuard(p)},{}},{{"pull",p,narrow(destination)},{} }},300000,true};}
static std::string absent(const std::string& path){auto q=shellQuote(path);return "if test -e "+q+" || test -L "+q+"; then echo 'La destination existe déjà' >&2; exit 1; fi";}
JobRequest pushRemoteFile(const std::wstring& source,const std::string& destination,bool overwrite){
    auto p=remotePath(destination);if(p=="/")throw std::runtime_error("Un fichier de destination est requis.");
    std::error_code ec;if(!std::filesystem::is_regular_file(source,ec))throw std::runtime_error("Le fichier local est absent ou inaccessible.");
    static std::atomic<uint64_t> sequence{0};auto suffix=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence);
    auto temp=remoteParent(p)+"/.questdev-upload-"+suffix;auto target=shellQuote(p),temporary=shellQuote(temp);
    auto guard="test ! -d "+target+" && test ! -L "+target+" || { echo 'Destination dossier ou lien refusée' >&2; exit 1; }; "+(overwrite?std::string(":"):absent(p));
    auto commit=guard+"; mv -T "+(overwrite?"-f":"-n")+" -- "+temporary+" "+target+" || exit $?; test ! -e "+temporary+" && test ! -L "+temporary;
    return {"Envoyer vers "+p,{{{"shell",guard},{}},{{"push",narrow(source),temp},{}},{{"shell",commit},{}}},300000,true,{{{"shell","rm -f -- "+temporary},{}}}};
}
JobRequest changeRemoteFile(const std::string& action,const std::string& path,const std::string& name){
    auto p=remotePath(path);const std::set<std::string> roots={"/","/sdcard","/storage","/storage/emulated","/storage/emulated/0","/sdcard/Android","/sdcard/Android/data","/sdcard/Android/obb","/data","/data/local","/data/local/tmp","/system","/vendor","/proc","/dev","/sys"};
    std::string script,label;
    if(action=="mkdir"){auto to=remoteChild(p,name);script="mkdir -- "+shellQuote(to);label="Créer "+to;}
    else {if(roots.count(p))throw std::runtime_error("Ce dossier racine ne peut pas être modifié ici.");auto q=shellQuote(p);
        if(action=="rename"){auto to=remoteChild(remoteParent(p),name);if(to==p)throw std::runtime_error("Le nouveau nom est identique.");script=absent(to)+"; mv -T -n -- "+q+" "+shellQuote(to)+" || exit $?; test ! -e "+q+" && test ! -L "+q;label="Renommer "+p+" → "+to;}
        else if(action=="delete"){script="if test -L "+q+" || test -f "+q+"; then rm -- "+q+"; elif test -d "+q+"; then rmdir -- "+q+"; else echo 'Type absent ou non pris en charge' >&2; exit 1; fi";label="Supprimer "+p;}
        else throw std::runtime_error("Action de fichier inconnue.");
    }return {label,{{{"shell",script},{}}},30000,true};
}
}
