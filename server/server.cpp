/*

  Copyright (C) 2011 Grame

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

  Grame Research Laboratory, 9 rue du Garet, 69001 Lyon - France
  research@grame.fr

*/

#include <QDir>
#include <QTextStream>

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <algorithm>

#include "utilities.h"
#include "server.h"
#include "guido2img.h"
#include "guidosession.h"

// json
#include "json/json_object.h"
#include "json/json_array.h"
#include "json/json_parser.h"
#include "json/json_stream.h"

#include "openssl/sha.h"

using namespace json;

using namespace std;
#define COOKIE_NAME "guidoserver"

namespace guidohttpd
{

//--------------------------------------------------------------------------
// static functions
// provided as callbacks to mhttpd
//--------------------------------------------------------------------------
static int _answer_to_connection (void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version,
                                  const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    HTTPDServer* server = (HTTPDServer*)cls;
    return server->answer(connection, url, method, version, upload_data, upload_data_size, con_cls);
}

//--------------------------------------------------------------------------

void _request_completed (void *cls, struct MHD_Connection *connection,
                         void **con_cls,
                         enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct *con_info = (connection_info_struct *)(*con_cls);

    if (NULL == con_info) {
        return;
    }

    if (con_info->connectiontype == POST) {
        MHD_destroy_post_processor (con_info->postprocessor);
        //if (con_info->answerstring) free (con_info->answerstring);
    }

    free (con_info);
    *con_cls = NULL;
}

static int _get_params (void *cls, enum MHD_ValueKind , const char *key, const char *data)
{
    TArgs* args = (TArgs*)cls;
    TArg arg(key, (data ? data : ""));
    args->insert (arg);
    return MHD_YES;
}


static int
_post_params (void *coninfo_cls, enum MHD_ValueKind , const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data,
              uint64_t off, size_t size)
{
    struct connection_info_struct *con_info = (connection_info_struct *)coninfo_cls;
    if (strcmp (key, "data") == 0) {
        if (con_info->args.find("data") != con_info->args.end()) {
          con_info->args["data"] += string(data);
        }
        else {
          con_info->args.insert(TArg("data", string(data)));
        }
    }
    return MHD_YES;
}

//--------------------------------------------------------------------------
// the http server
//--------------------------------------------------------------------------
HTTPDServer::HTTPDServer(int port, int verbose, string cachedir, guido2img* g2svg)
    : fPort(port), fVerbose(verbose), fCachedir(cachedir), fServer(0), fConverter(g2svg)
{
}

HTTPDServer::~HTTPDServer()
{
    for (map<string, guidosession *>::iterator it = fSessions.begin();
            it != fSessions.end(); it++) {
        delete it->second;
    }

    stop();
}

//--------------------------------------------------------------------------
bool HTTPDServer::start(int port)
{
    fServer = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, _answer_to_connection, this, MHD_OPTION_NOTIFY_COMPLETED, &_request_completed, NULL, MHD_OPTION_END);
    return fServer != 0;
}

//--------------------------------------------------------------------------
void HTTPDServer::stop ()
{
    if (fServer) {
        MHD_stop_daemon (fServer);
    }
    fServer=0;
}

//--------------------------------------------------------------------------
int HTTPDServer::send (struct MHD_Connection *connection, guidosessionresponse &response)
{
    return send (connection, response.data_, response.size_, response.format_.c_str(), response.http_status_);
}

//--------------------------------------------------------------------------
int HTTPDServer::send (struct MHD_Connection *connection, const char *page, int length, const char* type, int status)
{
    struct MHD_Response *response = MHD_create_response_from_buffer (length, (void *) page, MHD_RESPMEM_MUST_COPY);
    if (!response) {
        cerr << "MHD_create_response_from_buffer error: null response\n";
        return MHD_NO;
    }
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, type ? type : "text/plain");
    int ret = MHD_queue_response (connection, status, response);
    MHD_destroy_response (response);
    return ret;
}

//--------------------------------------------------------------------------
int HTTPDServer::send (struct MHD_Connection *connection, const char *page, const char* type, int status)
{
    return send (connection, page, strlen (page), type, status);
}

//--------------------------------------------------------------------------
const char* HTTPDServer::getMIMEType (const string& page)
{
    size_t n = page.find_last_of ('.');
    if (n != string::npos) {
        string ext = page.substr (n+1);
        if (ext == "css")	{
            return "text/css";
        }
        if (ext == "html")	{
            return "text/html";
        }
        if (ext == "js")	{
            return "application/javascript";
        }
    }
    return "text/plain";		// default MIME type
}

static const char alphanum[] =
"0123456789"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz";

int stringLength = sizeof(alphanum) - 1;

char genRandom()
{
    
    return alphanum[rand() % stringLength];
}

std::string generate_sha1(std::string setter)
{
  unsigned char obuf[20];
  SHA1((const unsigned char *)(setter.c_str()), setter.length(), obuf);
  char buffer[20];
  std::stringstream ss;

  for (int i=0; i < 20; i++) {
     sprintf(buffer, "%02x", obuf[i]);
     ss << buffer;
  }

  return ss.str();
}

void HTTPDServer::readFromCache()
{
  QDir myDir(fCachedir.c_str());
  QStringList filesList = myDir.entryList();

  for (int i = 0; i < filesList.size(); i++) {
    string unique_id = filesList[i].toStdString();
    QFile file((fCachedir+"/"+unique_id+"/"+unique_id+".gmn").c_str());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
                
    QTextStream in(&file);
    string all = in.readAll().toStdString();
    file.close();

    registerGMN(unique_id, all);
  }
}

void HTTPDServer::registerGMN(string unique_id, string gmn)
{
    if (fSessions.find (unique_id) == fSessions.end ()) {
      fSessions[unique_id] = new guidosession(fConverter, gmn, unique_id);
      fSessions[unique_id]->initialize();
    }
    QDir dir((fCachedir+'/'+unique_id).c_str());
    if (!dir.exists()) {
      dir.mkpath(".");
      QFile file(dir.absoluteFilePath((unique_id+".gmn").c_str()));
      if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << gmn.c_str();
        file.close();
      }
    }
}

int HTTPDServer::sendGuidoPostRequest(struct MHD_Connection *connection, const TArgs& args)
{
    if (args.size() != 1) {
        guidosessionresponse response = guidosession::genericFailure("Requests without scores MUST only contain one field called `data'.", 403);
        return send (connection, response);
    }
    if (args.begin()->first != "data") {
        guidosessionresponse response = guidosession::genericFailure("Requests without scores MUST only contain one field called `data'.", 403);
        return send (connection, response);
    }
    // we verify to see if this is valid guido
    GuidoErrCode err = guidosession::verifyGMN(args.begin()->second);
    if (err != guidoNoErr) {
        guidosessionresponse response = guidosession::genericFailure("You have sent the server invalid GMN.", 400);
        return send (connection, response);
    }
    std::string unique_id;
    /*
    // old method of making a random hash
    for(unsigned int i = 0; i < 20; ++i) {
        unique_id += genRandom();
    }
    */
    unique_id = generate_sha1(args.begin()->second);
    // if the score does not exist already, we put it there
    registerGMN(unique_id, args.begin()->second);
    guidosessionresponse response = fSessions[unique_id]->genericReturnId();
    return send (connection, response);
}

int HTTPDServer::sendGuidoDeleteRequest(struct MHD_Connection *connection, const TArgs& args)
{
    guidosessionresponse response;
    if (!args.size () || args.size () > 1 || args.begin()->first != "ID") {
        response.errstring_ = "There may be one and only one argument called \"ID\" passed to DELETE.";
        response.data_ = response.errstring_.c_str();
        response.size_ = response.errstring_.size();
        response.http_status_ = 405;
    } else if (fSessions.find (args.begin()->second) == fSessions.end ()) {
        response.errstring_ = "Cannot delete the score with ID "+(args.begin()->second)+" because it does not exist.";
        response.data_ = response.errstring_.c_str();
        response.size_ = response.errstring_.size();
        response.http_status_ = 404;
    } else {
        guidosession *toErase = fSessions[args.begin()->second];
        fSessions.erase (args.begin()->second);
        delete toErase;
        response.errstring_ = "";
        string success = "Successfully removed the score with ID "+(args.begin()->second)+".";
        response.data_ = success.c_str();
        response.size_ = success.size();
        response.http_status_ = 200;
    }
    return send (connection, response);
}
    
//--------------------------------------------------------------------------
int HTTPDServer::sendGuido (struct MHD_Connection *connection, const char* url, const TArgs& args, int type)
{
    /*
       there are three possibilities for the URL : 0, 1, and 2 entries
       in the case of 0, we must be making a new score and the request must be POST
       in the case of 1, we are referencing an existing score and the request must be either GET or DELETE
       in the case of 2, we are referencing an existing score with a precise operation and the request must be GET
    */
    
    // LOGFILE.
    const string stypes[4] = {"GET", "POST", "DELETE", "HEAD"};
    string stype = stypes[type];
    if (((type == GET) && (fVerbose == 1)) || (fVerbose == 2)) {
      log << stype << logend;
      log << url << logend;
      for(TArgs::const_iterator it = args.begin(); it != args.end(); it++) {
        log << it->first << logend;
        log << it->second << logend;
      }
    }

    // first, parse the URL
    std::stringstream ss(url);
    std::string item;
    vector<string> elems;
    while (getline(ss, item, '/')) {
        if (item != "") {
            elems.push_back(item);
        }
    }

    if (!elems.size()) {
        if (type != POST) {
            guidosessionresponse response = guidosession::genericFailure("Requests without scores MUST be POST.", 403);
            return send (connection, response);
        }
        return sendGuidoPostRequest(connection, args);
    }

    if (elems[0] == "version") {
        guidosessionresponse response = guidosession::handleSimpleStringQuery("version", guidosession::getVersion());
        return send(connection, response);
    } else if (elems[0] == "server") {
        guidosessionresponse response = guidosession::handleSimpleStringQuery("server", guidosession::getServerVersion());
        return send(connection, response);
    } else if (elems[0] == "linespace") {
        guidosessionresponse response = guidosession::handleSimpleFloatQuery("linespace", guidosession::getLineSpace());
        return send(connection, response);
    }

    map<string, guidosession *>::iterator it = fSessions.find(elems[0]);
    if (it == fSessions.end ()) {
        guidosessionresponse response = guidosession::genericFailure("incorrect score ID.", 404, elems[0]);
        return send (connection, response);
    }
    guidosession *currentSession = fSessions[elems[0]];

    if (type == DELETE) {
        // delete an fSession.
        sendGuidoDeleteRequest(connection, args);
    } else if ((type == GET) || (type == HEAD)) {
        currentSession->updateValuesFromDefaults(args);
        if (elems.size() == 1) {
            // must be getting the score
            guidosessionresponse response = currentSession->genericReturnImage();
            return send (connection, response);
        }
        if (elems.size() == 2) {
            // the second element will always specify something we need in JSON
            // is there a way to streamline this...a lot of logic dup but not much code dup...
            // maybe templates...but it is sufficiently different to perhaps warrant writing
            // all this stuff out
            if (elems[1] == "voicescount") {
                int nvoices = currentSession->voicesCount();
                guidosessionresponse response = nvoices >= 0
                    ? currentSession->handleSimpleIDdIntQuery("voicescount", nvoices)
                    : guidosession::genericFailure("Could not get the number of voices from this score.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "pagescount") {
                int npages = currentSession->pagesCount();
                guidosessionresponse response = npages >= 0
                    ? currentSession->handleSimpleIDdIntQuery("pagescount", npages)
                    : guidosession::genericFailure("Could not get the number of pages from this score.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "duration") {
                string duration = currentSession->duration();
                guidosessionresponse response = duration != ""
                    ? currentSession->handleSimpleIDdStringQuery("duration", duration)
                    : guidosession::genericFailure("Could not get the duration of this score.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "pageat") {
                GuidoDate date;
                string mydate = "";
                if (args.find("date") != args.end()) {
                    mydate = args.find("date")->second;
                }
                stringToDate(mydate, date);
                int page = currentSession->pageAt(date);
                guidosessionresponse response = page >= 0
                    ? currentSession->datePageJson(mydate, page)
                    : guidosession::genericFailure("The score does not contain this date.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "pagedate") {
                GuidoDate date;
                int mypage = 1;
                if (args.find("page") != args.end()) {
                    mypage = atoi(args.find("page")->second.c_str());
                }
                int success = currentSession->pageDate(mypage, &date);
                guidosessionresponse response = success == 0
                    ? currentSession->datePageJson(dateToString(date), mypage)
                    : guidosession::genericFailure("This page does not exist in the score.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "pagemap") {
                Time2GraphicMap outmap;
                GuidoErrCode err;
                err = currentSession->getMap(PAGE, 0, outmap);
                guidosessionresponse response = err == guidoNoErr
                    ? currentSession->mapJson("pagemap", outmap)
                    : guidosession::genericFailure("Could not generate a page map.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "midi") {
                guidosessionresponse response = currentSession->genericReturnMidi();
                return send(connection, response);
            } else if (elems[1] == "systemmap") {
                Time2GraphicMap outmap;
                GuidoErrCode err;
                err = currentSession->getMap(SYSTEM, 0, outmap);
                guidosessionresponse response = err == guidoNoErr
                    ? currentSession->mapJson("systemmap", outmap)
                    : guidosession::genericFailure("Could not generate a system map.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "staffmap") {
                Time2GraphicMap outmap;
                GuidoErrCode err;
                int mystaff = 1;
                if (args.find("staff") != args.end()) {
                    mystaff = atoi(args.find("staff")->second.c_str());
                }
                err = currentSession->getMap(STAFF, mystaff, outmap);
                guidosessionresponse response = err == guidoNoErr
                    ? currentSession->mapJson("staffmap", outmap)
                    : guidosession::genericFailure("Could not generate a staff map.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "voicemap") {
                Time2GraphicMap outmap;
                GuidoErrCode err;
                int myvoice = 1;
                if (args.find("voice") != args.end()) {
                    myvoice = atoi(args.find("voice")->second.c_str());
                }
                err = currentSession->getMap(STAFF, myvoice, outmap);
                guidosessionresponse response = err == guidoNoErr
                    ? currentSession->mapJson("voicemap", outmap)
                    : guidosession::genericFailure("Could not generate a voice map.", 400, elems[0]);
                return send(connection, response);
            } else if (elems[1] == "timemap") {
                GuidoServerTimeMap outmap;
                GuidoErrCode err;
                err = currentSession->getTimeMap(outmap);
                guidosessionresponse response = err == guidoNoErr
                    ? currentSession->timeMapJson(outmap)
                    : guidosession::genericFailure("Could not generate a time map.", 400, elems[0]);
                return send(connection, response);
                //guidosessionresponse response = guidosession::genericFailure("timemap is not implemented yet but will be soon.", 501);
                //return send(connection, response);
            } else {
                guidosessionresponse response = guidosession::genericFailure("Unidentified GET request.", 404, elems[0]);
                return send(connection, response);
            }
        }
    }

    guidosessionresponse reallyBadResponse = guidosession::genericFailure("Only GET and DELETE requests may be sent to an already completed score.", 400);
    return send (connection, reallyBadResponse);
}

//--------------------------------------------------------------------------
int HTTPDServer::answer (struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    if (NULL == *con_cls) {
        struct connection_info_struct *con_info = new connection_info_struct ();
        if (0 == strcmp (method, "POST")) {
            con_info->postprocessor =
                MHD_create_post_processor (connection, 1024, // arbitrary, recommeneded by libmicrohttpd
                                           _post_params, (void *) con_info);

            if (NULL == con_info->postprocessor) {
                delete con_info;
                return MHD_NO;
            }
            /*
              The connectiontype field of con_info currently does nothing:
             it's a placeholder for using POST/GET distinctions as the server
             becomes more sophisticated.

             */
            con_info->connectiontype = POST;
        } else {
            con_info->connectiontype = GET;
        }

        *con_cls = (void *) con_info;

        return MHD_YES;
    }

    if (0 == strcmp (method, "POST")) {
        struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

        if (*upload_data_size != 0) {
            MHD_post_process (con_info->postprocessor, upload_data,
                              *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        } else {
            struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;
            return sendGuido (connection, url, con_info->args, POST);
        }
    }
    else if (0 == strcmp (method, "GET")) {
        TArgs args;
        MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, _get_params, &args);
        return sendGuido (connection, url, args, GET);
    }
    else if (0 == strcmp (method, "HEAD")) {
        TArgs args;
        MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, _get_params, &args);
        return sendGuido (connection, url, args, HEAD);
    }
    else if (0 == strcmp (method, "DELETE")) {
        TArgs args;
        MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, _get_params, &args);
        return sendGuido (connection, url, args, DELETE);
    }
    else {
      stringstream ss;
      ss << "The GUIDO server does not support the ";
      ss << method;
      ss << " command";
      guidosessionresponse reallyBadResponse = guidosession::genericFailure(ss.str ().c_str (), 400);
      return send (connection, reallyBadResponse);
    }
    // should never get here
    return MHD_NO;
}

} // end namespoace