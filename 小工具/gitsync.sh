git pull && 
git diff --name-status --full-index > diff.tmp
git add . &&
git commit -m "$(date)\n$(cat diff.tmp)" && 
git push && 
echo 'SUCCESS' && 
exit 0 || 
echo 'FAILED' && 
exit -1
