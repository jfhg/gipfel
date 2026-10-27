package org.flpsed.wipfel

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebView
import android.webkit.WebViewClient

class MainActivity : Activity() {

    private lateinit var web: WebView
    private var server: AssetServer? = null
    private var filePathCallback: ValueCallback<Array<Uri>>? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        web = WebView(this)
        setContentView(web)
        with(web.settings) {
            javaScriptEnabled = true
            domStorageEnabled = true          // localStorage: profiles & settings
            useWideViewPort = true            // honour the viewport meta tag
            loadWithOverviewMode = true
            mediaPlaybackRequiresUserGesture = false
        }
        web.webViewClient = WebViewClient()
        web.webChromeClient = object : WebChromeClient() {
            override fun onShowFileChooser(
                view: WebView,
                callback: ValueCallback<Array<Uri>>,
                params: FileChooserParams
            ): Boolean {
                filePathCallback?.onReceiveValue(null)
                filePathCallback = callback
                // params.createIntent() carries the accept="image/*" filter
                startActivityForResult(params.createIntent(), REQ_FILE)
                return true
            }
        }

        val srv = AssetServer(assets)
        server = srv
        web.loadUrl("http://127.0.0.1:${srv.port}/index.html")

        requestMediaPermissions()
    }

    /**
     * The reason this wrapper exists: with ACCESS_MEDIA_LOCATION granted the
     * OS hands us the ORIGINAL files (GPS EXIF intact) instead of the
     * location-redacted copies browsers are forced to see.
     */
    private fun requestMediaPermissions() {
        val needed = listOf(
            Manifest.permission.READ_MEDIA_IMAGES,
            Manifest.permission.ACCESS_MEDIA_LOCATION
        ).filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (needed.isNotEmpty()) {
            requestPermissions(needed.toTypedArray(), REQ_PERMS)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        // no special handling: without the grant the app still works,
        // the OS just redacts GPS EXIF exactly like in a browser
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == REQ_FILE) {
            val cb = filePathCallback
            filePathCallback = null
            val uris: Array<Uri>? = if (resultCode == RESULT_OK && data != null) {
                val clip = data.clipData
                when {
                    clip != null -> Array(clip.itemCount) { clip.getItem(it).uri }
                    data.data != null -> arrayOf(data.data!!)
                    else -> null
                }
            } else null
            cb?.onReceiveValue(uris)
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (web.canGoBack()) web.goBack() else @Suppress("DEPRECATION") super.onBackPressed()
    }

    override fun onDestroy() {
        server?.close()
        web.destroy()
        super.onDestroy()
    }

    companion object {
        private const val REQ_PERMS = 1
        private const val REQ_FILE = 2
    }
}
