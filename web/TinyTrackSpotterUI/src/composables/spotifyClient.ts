import axios from 'axios';
import zod from 'zod';
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
};

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

export const useSpotifyClient = () => ({
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
 fetchAccessToken: async (code: string, clientId: string, clientSecret: string) => {
    let body = 'grant_type=authorization_code';
    body += '&code=' + code;
    body += '&redirect_uri=' + encodeURI(window.location.origin);
    body += '&client_id=' + clientId;
    body += '&client_secret=' + clientSecret;
    return await callAuthorizationApi(body, clientId, clientSecret);
}
});

async function callAuthorizationApi (body: string, clientId: string, clientSecret: string) {
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


