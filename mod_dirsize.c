/*
 * mod_dirsize 0.1
 * Copyright (C) 2005 Gustavo Franco <stratus@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "util_script.h"

void ap_send_http_header(request_rec*);
int ap_rprintf(request_rec*, const char*, ...);
long ap_send_fb(BUFF *f, request_rec *r);

static void mod_dirsize_init(server_rec *s, pool *p) 
{
  ap_add_version_component("mod_dirsize/0.1");
}

static int dirsize(void *rp, child_info *pinfo)
{
  char **env;
  int child_pid;
  request_rec *r = (request_rec *) rp;

  env = ap_create_environment(r->pool, r->subprocess_env);
  ap_error_log2stderr(r->server);
  r->filename = "/usr/bin/du"; // fixme: ugly!
  r->args = ap_pstrcat(r->pool, "-sk+", r->path_info, NULL);
  ap_cleanup_for_exec();
  child_pid = ap_call_exec(r, pinfo, r->filename, env, 0);
#ifdef WIN32
  return(child_pid);
#else
  ap_log_error(APLOG_MARK, APLOG_ERR, NULL, "exec of %s failed", r->filename);
  exit(0);

  return(0);
#endif
}

int mod_dirsize_handler(request_rec *r)
{
  BUFF *pipe_output;
  char buf[MAX_STRING_LEN];
  char *sizeink=NULL;
  regmatch_t pmatch[2];
  
  r->path_info = ap_make_dirstr_parent(r->pool, r->filename);
  
  if(!ap_bspawn_child(r->pool,dirsize, (void *) r, kill_after_timeout,
		       NULL, &pipe_output, NULL)) {
     ap_log_error(APLOG_MARK, APLOG_ERR, r->server, 
		     "problems with dirsize subprocess");
     return HTTP_INTERNAL_SERVER_ERROR;
  }
  
  ap_bgets(buf, sizeof(buf), pipe_output);
  
  regex_t *cpat = ap_pregcomp(r->pool, "^(.+)\t", REG_EXTENDED);
  if(regexec(cpat, buf, cpat->re_nsub+1, pmatch, 0) == 0) {
     sizeink = ap_pregsub(r->pool, "$1", buf, cpat->re_nsub+1, pmatch); 
  }

#ifdef DEBUG  
  r->content_type = "text/html";
  ap_send_http_header(r);

  ap_rprintf(r, "<html>\n");
  ap_rprintf(r, "<head>\n");
  ap_rprintf(r, "<title>mod_dirsize</title>\n");
  ap_rprintf(r, "</head>\n");
  ap_rprintf(r, "<body>\n");
  ap_rprintf(r, "Request: %s<br>\n", r->the_request);
  ap_rprintf(r, "Server Hostname: %s<br>\n", r->server->server_hostname);
  ap_rprintf(r, "Server Admin: %s<br>\n", r->server->server_admin);
  ap_rprintf(r, "Filename: %s<br>\n", r->filename);
  ap_rprintf(r, "ServerRoot: %s<br>\n", ap_server_root_relative(r->pool, ""));
  ap_rprintf(r, "Path Info: %s<br>\n", r->path_info);
  
  ap_send_fb(pipe_output, r);

  ap_rprintf(r, "</body>\n");
  ap_rprintf(r, "</html>\n");
#else
  r->content_type = "text/xml";
  ap_send_http_header(r);
  
  ap_rprintf(r, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  ap_rprintf(r, "<dirsize xmlns:html=\"http://www.w3.org/1999/html\">\n");
  ap_rprintf(r, "<sizeink>%s</sizeink>\n", sizeink);
  ap_rprintf(r, "</dirsize>");
#endif
  
  ap_bclose(pipe_output);

  return(OK);
}

handler_rec mod_dirsize_handlers[] =
{
  {"mod_dirsize", mod_dirsize_handler},
  {NULL}
};

module MODULE_VAR_EXPORT dirsize_module = {
   STANDARD_MODULE_STUFF,
   mod_dirsize_init,			/* initializer */
   NULL,				/* dir config creater */
   NULL,				/* dir merger default is to override */
   NULL,				/* server config */
   NULL,				/* merge server config */
   NULL,				/* command table */
   mod_dirsize_handlers,		/* handlers */
   NULL,				/* filename translation */
   NULL,				/* check_user_id */
   NULL,				/* check auth */
   NULL,				/* check access */
   NULL,				/* type_checker */
   NULL,				/* fixups */
   NULL,				/* logger */
   NULL					/* header parser */
};
