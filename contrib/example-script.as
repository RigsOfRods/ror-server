// rorserver demo script
// ---------------------
// to run it, set "scriptfile=<script.as>" in config file,
// or run rorserver with "-script-file <script.as>" command line option.
//
// Documentation: https://github.com/RigsOfRods/ror-server/wiki/Scripting-reference
// Strings doc.:  https://www.angelcode.com/angelscript/sdk/docs/manual/doc_script_stdlib_string.html


// This will be invoked once on startup.
void main() {
    // This will be shown in main server log as |INFO|SCRIPT|
    server.Log("Hello angelscript!");
    
    HttpResponse response = server.httpRequest(
        "GET",                    // Method: GET/POST/PUT/DELETE
        "www.example.com",        // Host
        "/",                      // Server file path
        "text/plain",             // Content type, see https://en.wikipedia.org/wiki/Media_type and https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
        "");                      // Payload
    string logHttp = "HTTP response code: " + response.getCode();
    server.Log(logHttp);
}

