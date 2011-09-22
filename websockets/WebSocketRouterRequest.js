/************************************************************************
 *  Copyright 2010-2011 Worlize Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ***********************************************************************/
libs.WebSocketRouterRequest = function () {
    function WebSocketRouterRequest(webSocketRequest, resolvedProtocol) {
        this.webSocketRequest = webSocketRequest;
        if (resolvedProtocol === '____no_protocol____') {
            this.protocol = null;
        }
        else {
            this.protocol = resolvedProtocol;
        }
        this.origin = webSocketRequest.origin;
        this.resource = webSocketRequest.resource;
        this.httpRequest = webSocketRequest.httpRequest;
        this.remoteAddress = webSocketRequest.remoteAddress;
    };

    WebSocketRouterRequest.prototype.accept = function (origin) {
        return this.webSocketRequest.accept(this.protocol, origin);
    };

    WebSocketRouterRequest.prototype.reject = function (status, reason) {
        return this.webSocketRequest.reject(status, reason);
    };

    return WebSocketRouterRequest;
};