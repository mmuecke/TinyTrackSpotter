import { useAsyncState } from '@vueuse/core';
import { defineStore } from 'pinia';
import { useEspClient } from 'src/composables/espClient';
import { computed } from 'vue';

export const useEspStore = defineStore('espStore', () => {
  const client = useEspClient('http://spotifydisplay.local')
  const { isLoading: isLoadingClientId, state: clientId, isReady: isReadyClientId, execute: executeCientId } = useAsyncState(
    async () => {
      return await client.get('clientId')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  async function setClientId (clientId: string) {
    await client.set('clientId', clientId)
  }
  const { isLoading: isLoadingClientSecret, state: clientSecret, isReady: isReadyClientSecret, execute: executeClientSecret } = useAsyncState(
    async () => {
      return await client.get('clientSecret')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  async function setClientSecret (clientId: string) {
    await client.set('clientSecret', clientId)
  }
  const { isLoading: isLoadingRefreshToken, state: refreshToken, isReady: isReadyRefreshToken, execute: executeRefreshToken } = useAsyncState(
    async () => {
      return await client.get('refreshToken')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  async function setRefreshToken(clientId: string) {
    await client.set('refreshToken', clientId)
  }
  const isLoading = computed(() => isLoadingClientId.value || isLoadingClientSecret.value || isLoadingRefreshToken.value)
  const isReady = computed(() => isReadyClientId.value && isReadyClientSecret.value && isReadyRefreshToken.value)
  async function execute (delay?: number) {
    await executeCientId(delay)
    await executeClientSecret(delay)
    await executeRefreshToken(delay)
  }
  return {
    isLoading, clientId, clientSecret, refreshToken, isReady, execute, setClientId, setClientSecret, setRefreshToken
  }
});
