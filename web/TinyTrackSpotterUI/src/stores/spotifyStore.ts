import { useLocalStorage } from '@vueuse/core';
import { defineStore } from 'pinia';
import { Scope, Tokens, useSpotifyClient } from 'src/composables/spotifyClient';
import { reactive, readonly, toRefs } from 'vue';

export const useSpotifyStore = defineStore('sopitfy', () => {
  const _clientId = useLocalStorage<string | null>('clientId', null);
  const _clientSecret = useLocalStorage<string | null>('clientSecret', null);
  const _tokens = useLocalStorage<Tokens | null>('tokens', null, { serializer: {write: JSON.stringify, read: JSON.parse }});

  async function requestAuthorization (
    clientId: string,
    clientSecret: string,
    ...scopes: Scope[]
  ) {
    _clientId.value = clientId;
    _clientSecret.value = clientSecret;

    // await callSetSettingsApi('clientId', client_id)
    // localStorage.setItem("client_id", client_id);
    // await callSetSettingsApi('clientSecret', client_secret)
    // localStorage.setItem("client_secret", client_secret); // In a real app you should not expose your client_secret to the user

    useSpotifyClient(_clientId.value, _clientSecret.value, _tokens).authorize(clientId, ...scopes);
  }

  function setTokens (tokens: Tokens) {
    _tokens.value = tokens;
  }

  // function refreshAccessToken () {
  //   if (_clientId.value && _clientSecret.value && _tokens.value?.refresh_token) {
  //     useSpotifyClient(_clientId.value, _clientSecret.value, _tokens.value?.refresh_token).refreshAccessToken();
  //   }
  // }

  function useClient() {
    if (!_clientId.value || !_clientSecret.value) {
      throw new Error('Client not initialized');
    }

    return useSpotifyClient(_clientId.value, _clientSecret.value, _tokens);
  }

  return {
    clientId: _clientId,
    clientSecret: _clientSecret,
    tokens: readonly(_tokens),
    requestAuthorization,
    useClient,
    setTokens
  };
});
