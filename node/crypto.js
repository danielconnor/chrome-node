// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.
if(typeof Crypto=="undefined"||!Crypto.util)(function(){var i=window.Crypto={},n=i.util={rotl:function(a,c){return a<<c|a>>>32-c},rotr:function(a,c){return a<<32-c|a>>>c},endian:function(a){if(a.constructor==Number)return n.rotl(a,8)&16711935|n.rotl(a,24)&4278255360;for(var c=0;c<a.length;c++)a[c]=n.endian(a[c]);return a},randomBytes:function(a){for(var c=[];a>0;a--)c.push(Math.floor(Math.random()*256));return c},bytesToWords:function(a){for(var c=[],b=0,d=0;b<a.length;b++,d+=8)c[d>>>5]|=a[b]<<24-
d%32;return c},wordsToBytes:function(a){for(var c=[],b=0;b<a.length*32;b+=8)c.push(a[b>>>5]>>>24-b%32&255);return c},bytesToHex:function(a){for(var c=[],b=0;b<a.length;b++){c.push((a[b]>>>4).toString(16));c.push((a[b]&15).toString(16))}return c.join("")},hexToBytes:function(a){for(var c=[],b=0;b<a.length;b+=2)c.push(parseInt(a.substr(b,2),16));return c},bytesToBase64:function(a){if(typeof btoa=="function")return btoa(j.bytesToString(a));for(var c=[],b=0;b<a.length;b+=3)for(var d=a[b]<<16|a[b+1]<<
8|a[b+2],e=0;e<4;e++)b*8+e*6<=a.length*8?c.push("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".charAt(d>>>6*(3-e)&63)):c.push("=");return c.join("")},base64ToBytes:function(a){if(typeof atob=="function")return j.stringToBytes(atob(a));a=a.replace(/[^A-Z0-9+\/]/ig,"");for(var c=[],b=0,d=0;b<a.length;d=++b%4)d!=0&&c.push(("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".indexOf(a.charAt(b-1))&Math.pow(2,-2*d+8)-1)<<d*2|"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".indexOf(a.charAt(b))>>>
6-d*2);return c}};i=i.charenc={};i.UTF8={stringToBytes:function(a){return j.stringToBytes(unescape(encodeURIComponent(a)))},bytesToString:function(a){return decodeURIComponent(escape(j.bytesToString(a)))}};var j=i.Binary={stringToBytes:function(a){for(var c=[],b=0;b<a.length;b++)c.push(a.charCodeAt(b)&255);return c},bytesToString:function(a){for(var c=[],b=0;b<a.length;b++)c.push(String.fromCharCode(a[b]));return c.join("")}}})();
(function(){var i=Crypto,n=i.util,j=i.charenc,a=j.UTF8,c=j.Binary,b=i.SHA1=function(d,e){var f=n.wordsToBytes(b._sha1(d));return e&&e.asBytes?f:e&&e.asString?c.bytesToString(f):n.bytesToHex(f)};b._sha1=function(d){if(d.constructor==String)d=a.stringToBytes(d);var e=n.bytesToWords(d),f=d.length*8;d=[];var k=1732584193,g=-271733879,l=-1732584194,m=271733878,o=-1009589776;e[f>>5]|=128<<24-f%32;e[(f+64>>>9<<4)+15]=f;for(f=0;f<e.length;f+=16){for(var q=k,r=g,s=l,t=m,u=o,h=0;h<80;h++){if(h<16)d[h]=e[f+
h];else{var p=d[h-3]^d[h-8]^d[h-14]^d[h-16];d[h]=p<<1|p>>>31}p=(k<<5|k>>>27)+o+(d[h]>>>0)+(h<20?(g&l|~g&m)+1518500249:h<40?(g^l^m)+1859775393:h<60?(g&l|g&m|l&m)-1894007588:(g^l^m)-899497514);o=m;m=l;l=g<<30|g>>>2;g=k;k=p}k+=q;g+=r;l+=s;m+=t;o+=u}return[k,g,l,m,o]};b._blocksize=16;b._digestsize=20})();
(function(){var i=Crypto,n=i.util,j=i.charenc,a=j.UTF8,c=j.Binary;i.HMAC=function(b,d,e,f){if(d.constructor==String)d=a.stringToBytes(d);if(e.constructor==String)e=a.stringToBytes(e);if(e.length>b._blocksize*4)e=b(e,{asBytes:true});var k=e.slice(0);e=e.slice(0);for(var g=0;g<b._blocksize*4;g++){k[g]^=92;e[g]^=54}b=b(k.concat(b(e.concat(d),{asBytes:true})),{asBytes:true});return f&&f.asBytes?b:f&&f.asString?c.bytesToString(b):n.bytesToHex(b)}})();


var SecureContext;
var Hmac;
var Cipher;
var Decipher;
var Sign;
var Verify;
var DiffieHellman;
var PBKDF2;
var crypto = false;

function Hash(type) {
    this.type = type.toUpperCase();
    this.string = "";
}
Hash.prototype.update = function (data) {
    this.string += data;
}
Hash.prototype.digest = function () {
    return btoa(Crypto[this.type](this.string, { asString: true }));
}


function Credentials(secureProtocol, flags, context) {
    if (!(this instanceof Credentials)) {
        return new Credentials(secureProtocol);
    }

    if (!crypto) {
        throw new Error('node.js not compiled with openssl crypto support.');
    }

    if (context) {
        this.context = context;
    } else {
        this.context = new SecureContext();

        if (secureProtocol) {
            this.context.init(secureProtocol);
        } else {
            this.context.init();
        }
    }

    if (flags) this.context.setOptions(flags);
}

exports.Credentials = Credentials;


exports.createCredentials = function (options, context) {
    if (!options) options = {};

    var c = new Credentials(options.secureProtocol,
                      options.secureOptions,
                      context);

    if (context) return c;

    if (options.key) c.context.setKey(options.key);

    if (options.cert) c.context.setCert(options.cert);

    if (options.ciphers) c.context.setCiphers(options.ciphers);

    if (options.ca) {
        if (Array.isArray(options.ca)) {
            for (var i = 0, len = options.ca.length; i < len; i++) {
                c.context.addCACert(options.ca[i]);
            }
        } else {
            c.context.addCACert(options.ca);
        }
    } else {
        c.context.addRootCerts();
    }

    if (options.crl) {
        if (Array.isArray(options.crl)) {
            for (var i = 0, len = options.crl.length; i < len; i++) {
                c.context.addCRL(options.crl[i]);
            }
        } else {
            c.context.addCRL(options.crl);
        }
    }

    return c;
};


exports.Hash = Hash;
exports.createHash = function (hash) {
    return new Hash(hash);
};


exports.Hmac = Hmac;
exports.createHmac = function (hmac, key) {
    return (new Hmac).init(hmac, key);
};


exports.Cipher = Cipher;
exports.createCipher = function (cipher, password) {
    return (new Cipher).init(cipher, password);
};


exports.createCipheriv = function (cipher, key, iv) {
    return (new Cipher).initiv(cipher, key, iv);
};


exports.Decipher = Decipher;
exports.createDecipher = function (cipher, password) {
    return (new Decipher).init(cipher, password);
};


exports.createDecipheriv = function (cipher, key, iv) {
    return (new Decipher).initiv(cipher, key, iv);
};


exports.Sign = Sign;
exports.createSign = function (algorithm) {
    return (new Sign).init(algorithm);
};

exports.Verify = Verify;
exports.createVerify = function (algorithm) {
    return (new Verify).init(algorithm);
};

exports.DiffieHellman = DiffieHellman;
exports.createDiffieHellman = function (size_or_key, enc) {
    if (!size_or_key) {
        return new DiffieHellman();
    } else if (!enc) {
        return new DiffieHellman(size_or_key);
    } else {
        return new DiffieHellman(size_or_key, enc);
    }

}

exports.pbkdf2 = PBKDF2;
