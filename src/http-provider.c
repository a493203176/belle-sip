/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "belle_sip_internal.h"

typedef struct belle_http_channel_context belle_http_channel_context_t;

#define BELLE_HTTP_CHANNEL_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_http_channel_context_t)

struct belle_http_channel_context{
	belle_sip_object_t base;
	belle_sip_list_t *pending_requests;
};

struct belle_http_provider{
	belle_sip_stack_t *stack;
	char *bind_ip;
	belle_sip_list_t *tcp_channels;
	belle_sip_list_t *tls_channels;
};

static int channel_on_event(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, unsigned int revents){
	if (revents & BELLE_SIP_EVENT_READ){
	}
	return 0;
}

static int channel_on_auth_requested(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, const char* distinguished_name){
	if (BELLE_SIP_IS_INSTANCE_OF(chan,belle_sip_tls_channel_t)) {
		/*
		belle_http_provider_t *prov=BELLE_SIP_HTTP_PROVIDER(obj);
		belle_sip_auth_event_t* auth_event = belle_sip_auth_event_create(NULL,NULL);
		belle_sip_tls_channel_t *tls_chan=BELLE_SIP_TLS_CHANNEL(chan);
		auth_event->mode=BELLE_SIP_AUTH_MODE_TLS;
		belle_sip_auth_event_set_distinguished_name(auth_event,distinguished_name);
		
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_auth_requested,auth_event);
		belle_sip_tls_channel_set_client_certificates_chain(tls_chan,auth_event->cert);
		belle_sip_tls_channel_set_client_certificate_key(tls_chan,auth_event->key);
		belle_sip_auth_event_destroy(auth_event);
		*/
	}
	return 0;
}

static void channel_on_sending(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_http_channel_context_t *ctx=BELLE_HTTP_CHANNEL_CONTEXT(obj);
	ctx->pending_requests=belle_sip_list_append(ctx->pending_requests,msg);
}

static void channel_state_changed(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
	switch(state){
		case BELLE_SIP_CHANNEL_INIT:
		case BELLE_SIP_CHANNEL_RES_IN_PROGRESS:
		case BELLE_SIP_CHANNEL_RES_DONE:
		case BELLE_SIP_CHANNEL_CONNECTING:
		case BELLE_SIP_CHANNEL_READY:
		case BELLE_SIP_CHANNEL_RETRY:
			break;
		case BELLE_SIP_CHANNEL_ERROR:
		case BELLE_SIP_CHANNEL_DISCONNECTED:
			break;
	}
}

static void belle_http_channel_context_uninit(belle_http_channel_context_t *obj){
}

belle_http_channel_context_t * belle_http_channel_context_new(belle_sip_channel_t *chan){
	belle_http_channel_context_t *obj=belle_sip_object_new(belle_http_channel_context_t);
	belle_sip_channel_add_listener(chan,(belle_sip_channel_listener_t*)obj);
	return obj;
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_http_channel_context_t,belle_sip_channel_listener_t)
	channel_state_changed,
	channel_on_event,
	channel_on_sending,
	channel_on_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_http_channel_context_t,belle_sip_channel_listener_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_http_channel_context_t,belle_sip_object_t,belle_http_channel_context_uninit,NULL,NULL,FALSE);

static void http_provider_uninit(belle_http_provider_t *obj){
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_http_provider_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_http_provider_t,belle_sip_object_t,http_provider_uninit,NULL,NULL,FALSE);

belle_http_provider_t *belle_http_provider_new(belle_sip_stack_t *s, const char *bind_ip){
	belle_http_provider_t *p=belle_sip_object_new(belle_http_provider_t);
	p->stack=s;
	p->bind_ip=belle_sip_strdup(bind_ip);
	return p;
}

int belle_http_provider_send_request(belle_http_provider_t *obj, belle_http_request_t *req, belle_http_request_listener_t *listener){
	belle_sip_channel_t *chan;
	belle_sip_hop_t *hop=NULL;//belle_sip_hop_new_from_uri(belle_http_request_get_url(req));
	belle_sip_list_t **channels=NULL;
	
	if (strcasecmp(hop->transport,"tcp")==0) channels=&obj->tcp_channels;
	else if (strcasecmp(hop->transport,"tls")==0) channels=&obj->tls_channels;
	else{
		belle_sip_error("belle_http_provider_send_request(): unsupported transport %s",hop->transport);
		return -1;
	}
	
	belle_http_request_set_listener(req,listener);
	
	chan=belle_sip_channel_find_from_list(*channels,hop);
	
	if (!chan){
		chan=belle_sip_stream_channel_new_client(obj->stack,obj->bind_ip,0,hop->cname,hop->host,hop->port);
		belle_http_channel_context_new(chan);
		*channels=belle_sip_list_prepend(*channels,chan);
	}
	belle_sip_object_unref(hop);
	belle_sip_channel_queue_message(chan,BELLE_SIP_MESSAGE(req));
	return 0;
}
