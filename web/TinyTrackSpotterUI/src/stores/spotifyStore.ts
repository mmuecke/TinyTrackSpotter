import { useLocalStorage } from '@vueuse/core';
import { defineStore } from 'pinia';
import { Scope, Tokens, useSpotifyClient } from 'src/composables/spotifyClient';

export const useSpotifyStore = defineStore('sopitfy', () => {
  const _clientId = useLocalStorage<string | null>('clientId', null);
  const _clientSecret = useLocalStorage<string | null>('clientSecret', null);
  const _tokens = useLocalStorage<Tokens | null>('tokens', null);

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

    useSpotifyClient().authorize(clientId, ...scopes);
  }

  function setTokens (tokens: Tokens) {
    _tokens.value = tokens;
  }

  return {
    clientId: _clientId,
    clientSecret: _clientSecret,
    requestAuthorization,
    setTokens
  };
});
