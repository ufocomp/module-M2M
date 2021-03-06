/*++

Program name:

  Apostol Web Service

Module Name:

  M2M.cpp

Notices:

  Module: MTS Communicator M2M XML API

Author:

  Copyright (c) Prepodobny Alen

  mailto: alienufo@inbox.ru
  mailto: ufocomp@gmail.com

--*/

//----------------------------------------------------------------------------------------------------------------------

#include "Core.hpp"
#include "M2M.hpp"
//----------------------------------------------------------------------------------------------------------------------

#include "jwt.h"
//----------------------------------------------------------------------------------------------------------------------

#include "rapidxml.hpp"

using namespace rapidxml;
//----------------------------------------------------------------------------------------------------------------------

extern "C++" {

namespace Apostol {

    namespace Workers {

        void XMLToJSON(xml_node<> *node, CJSON &Json) {
            if (node == nullptr)
                return;

            if (node->type() == node_element) {
                CJSONValue Value;

                XMLToJSON(node->first_node(), Value);

                if (node->value_size() != 0) {
                    Json.Object().AddPair(node->name(), node->value());
                } else {
                    Json.Object().AddPair(node->name(), Value);
                }

                XMLToJSON(node->next_sibling(), Json);
            }
        };

        //--------------------------------------------------------------------------------------------------------------

        //-- CSOAPProtocol ---------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::JSONToSOAP(const CString &Action, const CJSON &Json, CString &xmlString) {

            const auto& Object = Json.Object();

            xmlString.Clear();

            xmlString =  R"(<?xml version="1.0" encoding="utf-8"?>)" LINEFEED;
            xmlString << R"(<soap12:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:soap12="http://www.w3.org/2003/05/soap-envelope">)" LINEFEED;
            xmlString << R"(  <soap12:Body>)" LINEFEED;
            xmlString << R"(    <)" << Action << R"( xmlns="http://mcommunicator.ru/M2M">)" LINEFEED;

            for (int i = 0; i < Object.Count(); i++) {
                const auto& Member = Object.Members(i);
                const auto& String = Member.String();
                const auto& Value = Member.Value().AsString();
                xmlString << CString().Format(R"(      <%s>%s</%s>)", String.c_str(), Value.c_str(), String.c_str()) << LINEFEED;
            }

            xmlString << R"(    </)" << Action << R"(>)" LINEFEED;
            xmlString << R"(  </soap12:Body>)" LINEFEED;
            xmlString << R"(</soap12:Envelope>)" LINEFEED;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CSOAPProtocol::SOAPToJSON(const CString &Action, const CString &xmlString, CJSON &Json) {
            xml_document<> xmlDocument;
            xmlDocument.parse<parse_default>((char *) xmlString.c_str());

            xml_node<> *Envelope = xmlDocument.first_node();
            xml_node<> *Body = Envelope->first_node();
            xml_node<> *Response = Body->first_node();

            XMLToJSON(Response->first_node(), Json);
        }

        //--------------------------------------------------------------------------------------------------------------

        //-- CM2M ------------------------------------------------------------------------------------------------------

        //--------------------------------------------------------------------------------------------------------------

        CM2M::CM2M(CModuleProcess *AProcess) : CApostolModule(AProcess, "m2m", "worker/m2m") {
            m_Headers.Add("Authorization");
            CM2M::InitMethods();
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::InitMethods() {
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoProxy(Connection); }));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , [this](auto && Connection) { DoOptions(Connection); }));
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, [this](auto && Connection) { MethodNotAllowed(Connection); }));
#else
            m_pMethods->AddObject(_T("POST")   , (CObject *) new CMethodHandler(true , std::bind(&CM2M::DoProxy, this, _1)));
            m_pMethods->AddObject(_T("OPTIONS"), (CObject *) new CMethodHandler(true , std::bind(&CM2M::DoOptions, this, _1)));
            m_pMethods->AddObject(_T("GET")    , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("HEAD")   , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PUT")    , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("DELETE") , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("TRACE")  , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("PATCH")  , (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
            m_pMethods->AddObject(_T("CONNECT"), (CObject *) new CMethodHandler(false, std::bind(&CM2M::MethodNotAllowed, this, _1)));
#endif
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoVerbose(CSocketEvent *Sender, CTCPConnection *AConnection, LPCTSTR AFormat, va_list args) {
            Log()->Debug(0, AFormat, args);
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CM2M::DoProxyExecute(CTCPConnection *AConnection) {

            auto LProxyConnection = dynamic_cast<CHTTPClientConnection*> (AConnection);
            auto LProxy = dynamic_cast<CHTTPProxy*> (LProxyConnection->Client());

            auto LRequest = LProxyConnection->Request();
            auto LReply = LProxyConnection->Reply();

            DebugReply(LReply);

            auto LConnection = LProxy->Connection();

            if (LConnection->Connected()) {

                const auto& Token = LConnection->Data()["Token"];
                const auto& Action = LConnection->Data()["Action"];
                const auto& Agent = LRequest->UserAgent;

                CStringList LRouts;
                SplitColumns(LRequest->Location.pathname, LRouts, '/');

                CJSON Json;
                CSOAPProtocol::SOAPToJSON(Action, LReply->Content, Json);

                if (LRouts.Count() >= 3 && LRouts[2] == "SendMessage") {
                    const auto& SendMessageResult = Json["SendMessageResult"].AsString();
                    if (!SendMessageResult.IsEmpty()) {
                        CJSON Payload;
                        Payload.Object().AddPair("SendMessageResult", SendMessageResult);
                    }
                }

                LConnection->CloseConnection(true);

                LConnection->Reply()->ContentType = CHTTPReply::json;
                LConnection->Reply()->Content = Json.ToString();

                LConnection->SendReply(LReply->Status, nullptr, true);
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoProxyException(CTCPConnection *AConnection, const Delphi::Exception::Exception &E) {
            auto LProxyConnection = dynamic_cast<CHTTPClientConnection *> (AConnection);
            auto LProxy = dynamic_cast<CHTTPProxy *> (LProxyConnection->Client());

            Log()->Error(APP_LOG_EMERG, 0, "[%s:%d] %s", LProxy->Host().c_str(), LProxy->Port(), E.what());
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoEventHandlerException(CPollEventHandler *AHandler, const Delphi::Exception::Exception &E) {
            auto LProxyConnection = dynamic_cast<CHTTPClientConnection *> (AHandler->Binding());
            DoProxyException(LProxyConnection, E);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoProxyConnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPClientConnection*> (Sender);
            if (LConnection != nullptr) {
                Log()->Message(_T("[%s:%d] M2M Proxy connected."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoProxyDisconnected(CObject *Sender) {
            auto LConnection = dynamic_cast<CHTTPClientConnection*> (Sender);
            if (LConnection != nullptr) {
                Log()->Message(_T("[%s:%d] M2M Proxy disconnected."), LConnection->Socket()->Binding()->PeerIP(),
                               LConnection->Socket()->Binding()->PeerPort());
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        CHTTPProxy *CM2M::GetProxy(CHTTPServerConnection *AConnection) {
            auto LProxy = m_ProxyManager.Add(AConnection);
#if defined(_GLIBCXX_RELEASE) && (_GLIBCXX_RELEASE >= 9)
            LProxy->OnVerbose([this](auto && Sender, auto && AConnection, auto && AFormat, auto && args) { DoVerbose(Sender, AConnection, AFormat, args); });

            LProxy->OnExecute([this](auto && AConnection) { return DoProxyExecute(AConnection); });

            LProxy->OnException([this](auto && AConnection, auto && AException) { DoProxyException(AConnection, AException); });
            LProxy->OnEventHandlerException([this](auto && AHandler, auto && AException) { DoEventHandlerException(AHandler, AException); });

            LProxy->OnConnected([this](auto && Sender) { DoProxyConnected(Sender); });
            LProxy->OnDisconnected([this](auto && Sender) { DoProxyDisconnected(Sender); });
#else
            LProxy->OnVerbose(std::bind(&CM2M::DoVerbose, this, _1, _2, _3, _4));

            LProxy->OnExecute(std::bind(&CM2M::DoProxyExecute, this, _1));

            LProxy->OnException(std::bind(&CM2M::DoProxyException, this, _1, _2));
            LProxy->OnEventHandlerException(std::bind(&CM2M::DoEventHandlerException, this, _1, _2));

            LProxy->OnConnected(std::bind(&CM2M::DoProxyConnected, this, _1));
            LProxy->OnDisconnected(std::bind(&CM2M::DoProxyDisconnected, this, _1));
#endif
            return LProxy;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::VerifyToken(const CString &Token) {

            auto decoded = jwt::decode(Token);

            const auto& aud = CString(decoded.get_audience());
            const auto& alg = CString(decoded.get_algorithm());
            const auto& iss = CString(decoded.get_issuer());

            const auto& Providers = Server().Providers();

            CString Application;
            const auto Index = OAuth2::Helper::ProviderByClientId(Providers, aud, Application);
            if (Index == -1)
                throw COAuth2Error(_T("Not found provider by Client ID."));

            const auto& Provider = Providers[Index].Value();
            const auto& Secret = OAuth2::Helper::GetSecret(Provider, Application);

            CStringList Issuers;
            Provider.GetIssuers(Application, Issuers);
            if (Issuers[iss].IsEmpty())
                throw jwt::token_verification_exception("Token doesn't contain the required issuer.");

            if (alg == "HS256") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS384") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs384{Secret});
                verifier.verify(decoded);
            } else if (alg == "HS512") {
                auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs512{Secret});
                verifier.verify(decoded);
            }
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CM2M::CheckAuthorizationData(CHTTPRequest *ARequest, CAuthorization &Authorization) {

            const auto &LHeaders = ARequest->Headers;
            const auto &LCookies = ARequest->Cookies;

            const auto &LAuthorization = LHeaders.Values(_T("Authorization"));

            if (LAuthorization.IsEmpty()) {

                const auto &headerSession = LHeaders.Values(_T("Session"));
                const auto &headerSecret = LHeaders.Values(_T("Secret"));

                Authorization.Username = headerSession;
                Authorization.Password = headerSecret;

                if (Authorization.Username.IsEmpty() || Authorization.Password.IsEmpty())
                    return false;

                Authorization.Schema = CAuthorization::asBasic;
                Authorization.Type = CAuthorization::atSession;

            } else {
                Authorization << LAuthorization;
            }

            return true;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CM2M::CheckAuthorization(CHTTPServerConnection *AConnection, CAuthorization &Authorization) {

            auto LRequest = AConnection->Request();

            try {
                if (CheckAuthorizationData(LRequest, Authorization)) {
                    if (Authorization.Schema == CAuthorization::asBearer) {
                        VerifyToken(Authorization.Token);
                        return true;
                    }
                }

                if (Authorization.Schema == CAuthorization::asBasic)
                    AConnection->Data().Values("Authorization", "Basic");

                ReplyError(AConnection, CHTTPReply::unauthorized, "Unauthorized.");
            } catch (jwt::token_expired_exception &e) {
                ReplyError(AConnection, CHTTPReply::forbidden, e.what());
            } catch (jwt::token_verification_exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (CAuthorizationError &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            } catch (std::exception &e) {
                ReplyError(AConnection, CHTTPReply::bad_request, e.what());
            }

            return false;
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::DoProxy(CHTTPServerConnection *AConnection) {

            auto LRequest = AConnection->Request();
            auto LReply = AConnection->Reply();

            LReply->ContentType = CHTTPReply::json;

            auto LProxy = GetProxy(AConnection);
            auto LProxyRequest = LProxy->Request();

            CStringList LRouts;
            SplitColumns(LRequest->Location.pathname, LRouts, '/');

            if (LRouts.Count() < 2) {
                AConnection->SendStockReply(CHTTPReply::not_found);
                return;
            }

            CAuthorization LAuthorization;
            if (!CheckAuthorization(AConnection, LAuthorization))
                return;

            const auto& LProfile = LRequest->Params["profile"];
            const auto& profile = LProfile.IsEmpty() ? "main" : LProfile;

            const auto& uri = m_Profiles[profile]["uri"];
            const auto& apikey = m_Profiles[profile]["apikey"];
            const auto& naming = m_Profiles[profile]["naming"];

            const auto& Action = LRouts[1];

            AConnection->Data().Values("Token", LAuthorization.Token);
            AConnection->Data().Values("Action", Action);

            AConnection->CloseConnection(false);

            CJSON Json;
            ContentToJson(LRequest, Json);

            CLocation Location(uri);

            LProxy->Host() = Location.hostname;
            LProxy->Port(Location.port);
            LProxy->UsedSSL(Location.port == 443);

            const auto& LContentType = LRequest->Headers.Values("Content-Type");
            const auto& LUserAgent = LRequest->Headers.Values("User-Agent");

            LProxyRequest->Clear();

            LProxyRequest->Location = Location;
            LProxyRequest->UserAgent = LUserAgent;
            CSOAPProtocol::JSONToSOAP(Action, Json, LProxyRequest->Content);
            LProxyRequest->CloseConnection = true;

            CHTTPRequest::Prepare(LProxyRequest, "POST", Location.pathname.c_str(), "application/soap+xml; charset=utf-8");
            LProxyRequest->AddHeader("Authorization", "Bearer " + apikey);

            DebugRequest(LProxyRequest);

            LProxy->Active(true);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::InitConfig(const CIniFile &IniFile, const CString &Profile, CStringList &Config) {
            Config.AddPair("uri", IniFile.ReadString(Profile, "uri", "https://api.mcommunicator.ru/m2m/m2m_api.asmx"));
            Config.AddPair("apikey", IniFile.ReadString(Profile, "apikey", ""));
            Config.AddPair("naming", IniFile.ReadString(Profile, "naming", ""));
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::Initialization(CModuleProcess *AProcess) {
            CApostolModule::Initialization(AProcess);
            LoadConfig(Config()->IniFile().ReadString(SectionName().c_str(), "config", "conf/m2m.conf"), m_Profiles, InitConfig);
        }
        //--------------------------------------------------------------------------------------------------------------

        void CM2M::Heartbeat() {
            CApostolModule::Heartbeat();
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CM2M::Enabled() {
            if (m_ModuleStatus == msUnknown)
                m_ModuleStatus = Config()->IniFile().ReadBool(SectionName().c_str(), "enable", false) ? msEnabled : msDisabled;
            return m_ModuleStatus == msEnabled;
        }
        //--------------------------------------------------------------------------------------------------------------

        bool CM2M::CheckLocation(const CLocation &Location) {
            return Location.pathname.SubString(0, 5) == _T("/m2m/");
        }
    }
}
}