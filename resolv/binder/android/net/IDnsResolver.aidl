/**
 * Copyright (c) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.net;

import android.net.metrics.INetdEventListener;

/** {@hide} */
interface IDnsResolver {
    /**
     * Returns true if the service is responding.
     */
    boolean isAlive();

   /**
    * Register event listener
    * DnsResolver supports multiple event listeners, but only one per unique address of the
    * binder interface. A newer listener won't be registered if DnsResolver has an old one on
    * the same address of the binder interface.
    *
    * @param listener event listener to register.
    * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
    *         unix errno.
    */
    void registerEventListener(INetdEventListener listener);

    // Array indices for resolver parameters.
    const int RESOLVER_PARAMS_SAMPLE_VALIDITY = 0;
    const int RESOLVER_PARAMS_SUCCESS_THRESHOLD = 1;
    const int RESOLVER_PARAMS_MIN_SAMPLES = 2;
    const int RESOLVER_PARAMS_MAX_SAMPLES = 3;
    const int RESOLVER_PARAMS_BASE_TIMEOUT_MSEC = 4;
    const int RESOLVER_PARAMS_RETRY_COUNT = 5;
    const int RESOLVER_PARAMS_COUNT = 6;

    /**
     * Sets the name servers, search domains and resolver params for the given network. Flushes the
     * cache as needed (i.e. when the servers or the number of samples to store changes).
     *
     * @param netId the network ID of the network for which information should be configured.
     * @param servers the DNS servers to configure for the network.
     * @param domains the search domains to configure.
     * @param params the params to set. This array contains RESOLVER_PARAMS_COUNT integers that
     *   encode the contents of Bionic's __res_params struct, i.e. sample_validity is stored at
     *   position RESOLVER_PARAMS_SAMPLE_VALIDITY, etc.
     * @param tlsName The TLS subject name to require for all servers, or empty if there is none.
     * @param tlsServers the DNS servers to configure for strict mode Private DNS.
     * @param tlsFingerprints An array containing TLS public key fingerprints (pins) of which each
     *   server must match at least one, or empty if there are no pinned keys.
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void setResolverConfiguration(int netId, in @utf8InCpp String[] servers,
            in @utf8InCpp String[] domains, in int[] params,
            in @utf8InCpp String tlsName, in @utf8InCpp String[] tlsServers,
            in @utf8InCpp String[] tlsFingerprints);

    // Array indices for resolver stats.
    const int RESOLVER_STATS_SUCCESSES = 0;
    const int RESOLVER_STATS_ERRORS = 1;
    const int RESOLVER_STATS_TIMEOUTS = 2;
    const int RESOLVER_STATS_INTERNAL_ERRORS = 3;
    const int RESOLVER_STATS_RTT_AVG = 4;
    const int RESOLVER_STATS_LAST_SAMPLE_TIME = 5;
    const int RESOLVER_STATS_USABLE = 6;
    const int RESOLVER_STATS_COUNT = 7;

    /**
     * Retrieves the name servers, search domains and resolver stats associated with the given
     * network ID.
     *
     * @param netId the network ID of the network for which information should be retrieved.
     * @param servers the DNS servers that are currently configured for the network.
     * @param domains the search domains currently configured.
     * @param tlsServers the DNS-over-TLS servers that are currently configured for the network.
     * @param params the resolver parameters configured, i.e. the contents of __res_params in order.
     * @param stats the stats for each server in the order specified by RESOLVER_STATS_XXX
     *         constants, serialized as an int array. The contents of this array are the number of
     *         <ul>
     *           <li> successes,
     *           <li> errors,
     *           <li> timeouts,
     *           <li> internal errors,
     *           <li> the RTT average,
     *           <li> the time of the last recorded sample,
     *           <li> and an integer indicating whether the server is usable (1) or broken (0).
     *         </ul>
     *         in this order. For example, the timeout counter for server N is stored at position
     *         RESOLVER_STATS_COUNT*N + RESOLVER_STATS_TIMEOUTS
     * @param wait_for_pending_req_timeout_count an internal counter used to count the number of
     *        timeouts while resolver is handling concurrent DNS queries on the same hostname.
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     *
     * TODO: Consider replacing stats and params with parcelables.
     */
    void getResolverInfo(int netId, out @utf8InCpp String[] servers,
            out @utf8InCpp String[] domains, out @utf8InCpp String[] tlsServers, out int[] params,
            out int[] stats, out int[] wait_for_pending_req_timeout_count);

    /**
     * Starts NAT64 prefix discovery on the given network.
     *
     * @param netId the netId to start prefix discovery on.
     */
    void startPrefix64Discovery(int netId);

    /**
     * Stops NAT64 prefix discovery on the given network.
     *
     * @param netId the netId to stop prefix discovery on.
     */
    void stopPrefix64Discovery(int netId);

    /**
     * Get NAT64 prefix in format Pref64::/n which is described in RFC6147 section 2. This
     * interface is used for internal test only. Don't use it for other purposes because doing so
     * would cause race conditions with the NAT64 prefix notifications.
     *
     * @param netId the network ID of the network to get the prefix
     * @return the NAT64 prefix if the query operation was successful
     * @throws ServiceSpecificException in case of failure, with an error code indicating the
     *         cause of the the failure.
     *
     * TODO: Remove this once the tests have been updated to listen for onNat64PrefixEvent.
     */
    @utf8InCpp String getPrefix64(int netId);

    /**
     * Clear DNS servers and resolver configurations for the given network.
     *
     * @param netId the network ID of the network to clear.
     */
    void clearResolverConfiguration(int netId);
}
