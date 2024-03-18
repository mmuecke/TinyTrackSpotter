<template>
  <q-card>
    <q-markup-table flat bordered>
      <thead class="bg-primary">
        <tr>
          <th colspan="3">
            <div class="row no-wrap items-center">
              <div class="text-h4 q-ml-md text-white">Settings</div>
            </div>
          </th>
        </tr>
        <tr>
          <th class="text-left">Client Id</th>
          <th class="text-right">{{ clientIdLocal }}</th>
          <th class="text-right">{{ clientId }}</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td class="text-left">Client Secret</td>
          <td class="text-right">{{ clientSecretLocal }}</td>
          <td class="text-right">{{ clientSecret }}</td>
        </tr>
        <tr>
          <td class="text-left">Access Token</td>
          <td class="text-right" style="overflow: hidden; width: 50px;">{{ tokens?.access_token?.substring(0, 10) }}
          </td>
          <td class="text-right">---</td>
        </tr>
        <tr>
          <td class="text-left">Refresh Token</td>
          <td class="text-right" style="overflow: hidden; width: 50px;">{{ tokens?.refresh_token?.substring(0, 10) }}
          </td>
          <td class="text-right">{{ refreshToken?.substring ? refreshToken?.substring(0, 10) : '' }}</td>
        </tr>
      </tbody>
    </q-markup-table>
    <q-btn-group>
      <q-btn label="Load" @click="load" :loading="loading" />
      <q-btn label="Save" @click="submit" :loading="loading" />
      <q-btn label="Execute" @click="execute" :loading="loading" />
    </q-btn-group>
    <!-- <q-form @submit="onSubmit">
      <div>{{  }}</div>
      <q-input v-model:model-value="clientId" label="Client Id" />
      <q-input :model-value="clientSecret" label="Client Secret" />
      <q-input :model-value="accessToken" label="Access Token" />
      {{ tokens }}
      <q-input v-model="tokens.refresh_token" label="Refresh Token" />
      <q-input v-model="tokens.expires_in" label="Expires in" />
      <q-input v-model="tokens.token_type" label="Token Type" />
      <q-input v-model="tokens.scope" label="Scope" />
      <q-input v-model="tokens.expires_at" label="Expires at" />

    </q-form> -->
  </q-card>
</template>

<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { useEspStore } from 'src/stores/espStore';
import { useSpotifyStore } from 'src/stores/spotifyStore';
import { ref } from 'vue';

const espStore = useEspStore();
const spotifyStore = useSpotifyStore();

const { clientId, clientSecret, refreshToken } = storeToRefs(espStore);
const { tokens, clientId: clientIdLocal, clientSecret: clientSecretLocal } = storeToRefs(spotifyStore);

const loading = ref(false);
async function submit () {
  console.log('submit');
  try {
    loading.value = true;
    if (clientIdLocal.value)
      await espStore.setClientId(clientIdLocal.value);
    if (clientSecretLocal.value)
      await espStore.setClientSecret(clientSecretLocal.value);
    if (tokens.value?.refresh_token)
      await espStore.setRefreshToken(tokens.value?.refresh_token);
  } catch (error) {
    console.error(error);
  } finally {
    loading.value = false;
  }
}
async function load () {
  console.log('load');
  try {
    loading.value = true;
    clientIdLocal.value = clientId.value;
    clientSecretLocal.value = clientSecret.value;
    if (!tokens.value)
      refreshToken.value = { refresh_token: refreshToken.value };
  } finally {
    loading.value = false;
  }
}
async function execute () {
  try {
    loading.value = true;
    await espStore.execute();
  } finally {
    loading.value = false;
  }
}
</script>
