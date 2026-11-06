// External loader so the page needs no inline <script>, keeping
// `script-src 'self'` in the Content-Security-Policy sufficient.
import init from './gipfel.js';
init();
