package org.flpsed.wipfel

import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.ServerSocket
import java.net.Socket
import kotlin.concurrent.thread

/**
 * Minimal loopback HTTP server that serves the bundled assets.
 *
 * The WebView refuses fetch() on file:// origins, and WASM streaming
 * instantiation needs a real HTTP origin, so we serve the site from
 * 127.0.0.1 on an ephemeral port. Loopback only; nothing is exposed.
 */
class AssetServer(private val assetManager: android.content.res.AssetManager) : AutoCloseable {
    private val server = ServerSocket(0, 4, java.net.InetAddress.getByName("127.0.0.1"))
    val port: Int get() = server.localPort

    @Volatile
    private var running = true

    init {
        thread(isDaemon = true, name = "wipfel-http") {
            while (running) {
                try {
                    val socket = server.accept()
                    thread(isDaemon = true) { handle(socket) }
                } catch (_: IOException) {
                    running = false
                }
            }
        }
    }

    private fun handle(socket: Socket) {
        socket.use { s ->
            try {
                val input = s.getInputStream()
                val requestLine = input.readLine() ?: return
                val path = requestLine.split(" ").getOrNull(1) ?: "/"
                // read (and ignore) the remaining headers so the client is happy
                while (true) {
                    val line = input.readLine() ?: break
                    if (line.isEmpty()) break
                }
                val asset = if (path == "/" || path == "/index.html") "www/index.html"
                else "www/" + path.trimStart('/')
                val data = try {
                    assetManager.open(asset).use { it.readBytes() }
                } catch (_: IOException) {
                    respond(s, 404, "text/plain", "not found".toByteArray())
                    return
                }
                respond(s, 200, mime(asset), data)
            } catch (_: IOException) {
            }
        }
    }

    private fun respond(s: Socket, code: Int, ctype: String, body: ByteArray) {
        val out: OutputStream = s.getOutputStream()
        val header = buildString {
            append("HTTP/1.1 $code OK\r\n")
            append("Content-Type: $ctype\r\n")
            append("Content-Length: ${body.size}\r\n")
            append("Cache-Control: no-cache\r\n")
            append("Access-Control-Allow-Origin: *\r\n")
            append("Connection: close\r\n")
            append("\r\n")
        }
        out.write(header.toByteArray())
        out.write(body)
        out.flush()
    }

    private fun mime(path: String): String = when {
        path.endsWith(".wasm") -> "application/wasm"
        path.endsWith(".js") -> "application/javascript"
        path.endsWith(".css") -> "text/css"
        path.endsWith(".html") -> "text/html"
        path.endsWith(".gz") -> "application/gzip"
        else -> "application/octet-stream"
    }

    private fun InputStream.readLine(): String? {
        val buf = ByteArrayOutputStream(64)
        while (true) {
            val b = read()
            if (b == -1) return if (buf.size() == 0) null else buf.toString("ISO-8859-1")
            if (b == '\n'.code) return buf.toString("ISO-8859-1").trimEnd('\r')
            buf.write(b)
        }
    }

    override fun close() {
        running = false
        try { server.close() } catch (_: IOException) {}
    }
}
