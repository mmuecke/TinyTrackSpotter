import axios from 'axios';
import zod, { z } from 'zod';
import { diviceSchema } from './Divice';
import { PlaybackState, playbackStateSchema } from './PlaybackState';
import { RemovableRef } from '@vueuse/core';
const spotifyEndpoints = {
  authorize: 'https://accounts.spotify.com/authorize',
  token: 'https://accounts.spotify.com/api/token',
  playlists: 'https://api.spotify.com/v1/me/playlists',
  devices: 'https://api.spotify.com/v1/me/player/devices',
  play: 'https://api.spotify.com/v1/me/player/play',
  pause: 'https://api.spotify.com/v1/me/player/pause',
  next: 'https://api.spotify.com/v1/me/player/next',
  previous: 'https://api.spotify.com/v1/me/player/previous',
  player: 'https://api.spotify.com/v1/me/player',
  tracks: 'https://api.spotify.com/v1/playlists/{{PlaylistId}}/tracks',
  currentlyPlaying: 'https://api.spotify.com/v1/me/player/currently-playing',
  shuffle: 'https://api.spotify.com/v1/me/player/shuffle',
} as const;

export const scope = zod.union([
  zod.literal('user-read-private'),
  zod.literal('user-read-email'),
  zod.literal('user-modify-playback-state'),
  zod.literal('user-read-playback-position'),
  zod.literal('user-library-read'),
  zod.literal('streaming'),
  zod.literal('user-read-playback-state'),
  zod.literal('user-read-recently-played'),
  zod.literal('playlist-read-private'),
]);

export type Scope = zod.infer<typeof scope>;

export const _scopes: Scope[] = [
  'user-read-private',
  'user-read-email',
  'user-modify-playback-state',
  'user-read-playback-position',
  'user-library-read',
  'streaming',
  'user-read-playback-state',
  'user-read-recently-played',
  'playlist-read-private',
];

export const tokens = zod.object({
  access_token: zod.string().min(1),
  refresh_token: zod.string().min(1),
});
export type Tokens = zod.infer<typeof tokens>;
export class SpotifyClientError extends Error {
  constructor(message: string, public status: number) {
    super(message);
    this.name = 'SpotifyClientError';
  }
}
export const useSpotifyClient = (clientId: string, clientSecret: string, tokens:  RemovableRef<Tokens | null>) => {
  return ({
    authorize: (clientId: string, ...scopes: Scope[]) => {
      if (!scopes || scopes.length === 0) {
        scopes = _scopes;
      }
      const scope = scopes.join(' ');
      // const url = buildUrl(spotifyEndpoints.authorize, {
      //   client_id: clientId,
      //   response_type: 'code',
      //   redirect_uri: encodeURI(window.location.origin),
      //   show_dialog: 'true',
      //   scope,
      // });
      let url = spotifyEndpoints.authorize;
      url += '?client_id=' + clientId;
      url += '&response_type=code';
      url += '&redirect_uri=' + encodeURI(window.location.origin);
      url += '&show_dialog=true';
      url += '&scope=' + scope;

      window.location.href = url; // Show Spotify's authorization screen
    },
    fetchAccessToken: async (code: string) => {
      let body = 'grant_type=authorization_code';
      body += '&code=' + code;
      body += '&redirect_uri=' + encodeURI(window.location.origin);
      body += '&client_secret=' + clientSecret;
      tokens.value = await callAuthorizationApi(body, clientId, clientSecret);
    },
    refreshAccessToken: async () => {
      if (!tokens.value?.refresh_token) {
        throw new Error('No refresh token');
      }
      let body = 'grant_type=refresh_token';
      body += '&refresh_token=' + tokens.value?.refresh_token;
      tokens.value = await callAuthorizationApi(body, clientId, clientSecret);
    },
    getCurrentlyPlaying: async (): Promise<PlaybackState | null> => {
      const response = await axios.get(spotifyEndpoints.currentlyPlaying, getAxiosConfig());
      console.log('response', response);
      if (response.status == 200) {
        // return playbackStateSchema.parse(response.data);
        return response.data;
      }
      else if (response.status == 401 && tokens.value?.refresh_token) {
        useSpotifyClient(clientId, clientSecret, tokens).refreshAccessToken();
        return useSpotifyClient(clientId, clientSecret, tokens).getCurrentlyPlaying();
      }
      if (response.status == 204) {
        return null;
      }
      throw new SpotifyClientError(response.statusText, response.status);
    },
    getDevices: async () => {
      const response = await axios.get(spotifyEndpoints.devices, getAxiosConfig());
      if (response.status == 200) {
        return z.array(diviceSchema).parse(response.data);
      } else {
        throw new SpotifyClientError(response.statusText, response.status);
      }
    },
    getPlaylists: async () => {
      const response = await axios.get(spotifyEndpoints.playlists, getAxiosConfig());
      if (response.status == 200) {
        return response.data;
      } else {
        throw new SpotifyClientError(response.statusText, response.status);
      }
    }
  });
  function getAxiosConfig () {
    if (!tokens.value?.access_token) {
      throw new Error('No access token');
    }
  return {
    headers: {
      'Content-Type': 'application/json',
      'Authorization': 'Bearer ' + tokens.value?.access_token
    }
  }
}
};

async function callAuthorizationApi (body: string, clientId: string, clientSecret: string) {
  body += '&client_id=' + clientId;
  const response = await axios.post(spotifyEndpoints.token, body, {
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'Authorization': 'Basic ' + btoa(clientId + ':' + clientSecret)
    },
  })

  if(response.status == 200) {
    const data = response.data;
    return tokens.parse(data);
  } else {
    throw new Error(response.statusText);
  }
}


