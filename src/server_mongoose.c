#include <stdio.h>
#include <signal.h>

#include "mongoose/mongoose.h"
#include "server_mongoose.h"
#include "colors.h"

// To check for SIGKILL and SIGTERM auto exit
static volatile sig_atomic_t RUNNING = 1;
//
void SignalHandler(int signal) {
	/*
	This function catches any SIGKILL or SIGTERM, and then sets the
	signal RUNNING to false. Thus, killing the running server

	*/

	(void) signal;
	RUNNING = 0;
}

static void networkEventHandler(struct mg_connection* connection, int event, void* eventData) {
	/*
	This function handles network events, raised when the mongoose event manager catches
	any requests.

	Requires :
	connection -> (struct mg_connection*) a connection established when any client sends
				a request on the url.
	event -> event id (integer type)
	eventData -> (NULL) this will be assigned later dynamically
	*/
	
	if (event == MG_EV_HTTP_MSG) {
		// If event is a http msg, then we assigne the value to event data

		struct mg_http_message* httpMsg = (struct mg_http_message*) eventData;

		// Serving the static files from the configured directory (default is public/ for dev environment)
		struct mg_http_serve_opts Options = {.root_dir = "public/"};
		mg_http_serve_dir(connection, httpMsg, &Options);
	}
}

void startLocalServer(const char* host, int port) {
	/*
	 This function launches a local server in the public directory to render
	 the markdown file parsed.

	 Requires argument PORT -> string (const char*)
	*/

	// Setting up the signal handling
	signal(SIGINT, SignalHandler);

	// Disabling Mongoose logs output to tty
	// Set as per your requirement. Options are:
	// - - -
	// mg_log_set(MG_LL_NONE);
	mg_log_set(MG_LL_ERROR);
	// mg_log_set(MG_LL_INFO);
	// mg_log_set(MG_LL_DEBUG);
	// mg_log_set(MG_LL_VERBOSE);
	// mg_log_set(MG_LL_WARN);
	// - - -

	// Configuring the mongoose event manager
	struct mg_mgr MongooseEventManager;
	mg_mgr_init(&MongooseEventManager);
	char url[32];
	snprintf(url, sizeof(url), "%s:%d", host, port);

	// Starting the server to listen on url:port
	printf("[%s~%s] %sVisit: %s%s%s%s\n", YELLOW, DEFAULT, CYAN, DEFAULT, WHITE, url, DEFAULT);
	mg_http_listen(&MongooseEventManager, url, networkEventHandler, NULL);

	while (RUNNING) {
		// Running the server / listener unless a SIGKILL is registered

		mg_mgr_poll(&MongooseEventManager, 1000);
	}

	// Cleaning up
	mg_mgr_free(&MongooseEventManager);
}
