git pull && 
git add . &&
git commit -m "$(date)" --status && 
git push && 
echo 'SUCCESS' && 
exit 0 || 
echo 'FAILED' && 
exit -1
